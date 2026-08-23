#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <netinet/in.h>

#define PORT 80

#define USERNAME "admin"
#define PASSWORD "Hm361485%"

#define SESSION_TOKEN "PiMonitorSession_Hm361485_2026"

#define REQUEST_SIZE 8192
#define PROCESS_BUFFER 60000
#define PORT_BUFFER 30000

static unsigned long long previous_total = 0;
static unsigned long long previous_idle = 0;
static double cpu_usage = 0.0;


/* =========================================================
   CPU
   ========================================================= */

static unsigned long long read_cpu_total(void)
{
    FILE *f = fopen("/proc/stat", "r");

    if (!f)
        return 0;

    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;

    int r = fscanf(
        f,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &user,
        &nice,
        &system,
        &idle,
        &iowait,
        &irq,
        &softirq,
        &steal
    );

    fclose(f);

    if (r != 8)
        return 0;

    return user +
           nice +
           system +
           idle +
           iowait +
           irq +
           softirq +
           steal;
}


static unsigned long long read_cpu_idle(void)
{
    FILE *f = fopen("/proc/stat", "r");

    if (!f)
        return 0;

    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;

    int r = fscanf(
        f,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &user,
        &nice,
        &system,
        &idle,
        &iowait,
        &irq,
        &softirq,
        &steal
    );

    fclose(f);

    if (r != 8)
        return 0;

    return idle + iowait;
}


static void update_cpu(void)
{
    unsigned long long total = read_cpu_total();
    unsigned long long idle = read_cpu_idle();

    if (previous_total != 0)
    {
        unsigned long long total_delta =
            total - previous_total;

        unsigned long long idle_delta =
            idle - previous_idle;

        if (total_delta > 0)
        {
            cpu_usage =
                100.0 *
                (double)(total_delta - idle_delta) /
                (double)total_delta;
        }

        if (cpu_usage < 0)
            cpu_usage = 0;

        if (cpu_usage > 100)
            cpu_usage = 100;
    }

    previous_total = total;
    previous_idle = idle;
}


/* =========================================================
   Memory
   ========================================================= */

static unsigned long long meminfo_value(
    const char *wanted
)
{
    FILE *f = fopen("/proc/meminfo", "r");

    if (!f)
        return 0;

    char key[64];
    unsigned long long value;
    char unit[32];

    while (
        fscanf(
            f,
            "%63s %llu %31s",
            key,
            &value,
            unit
        ) == 3
    )
    {
        if (strcmp(key, wanted) == 0)
        {
            fclose(f);

            /*
             * /proc/meminfo is in KB.
             */
            return value * 1024ULL;
        }
    }

    fclose(f);

    return 0;
}


/* =========================================================
   Network
   ========================================================= */

static unsigned long long network_bytes(
    int receive
)
{
    FILE *f = fopen("/proc/net/dev", "r");

    if (!f)
        return 0;

    char line[512];
    char iface[64];

    unsigned long long rx;
    unsigned long long rx_packets;
    unsigned long long rx_errors;
    unsigned long long rx_drop;
    unsigned long long rx_fifo;
    unsigned long long rx_frame;
    unsigned long long rx_compressed;
    unsigned long long rx_multicast;

    unsigned long long tx;
    unsigned long long tx_packets;
    unsigned long long tx_errors;
    unsigned long long tx_drop;
    unsigned long long tx_fifo;
    unsigned long long tx_collisions;
    unsigned long long tx_carrier;
    unsigned long long tx_compressed;

    unsigned long long total = 0;

    fgets(line, sizeof(line), f);
    fgets(line, sizeof(line), f);

    while (fgets(line, sizeof(line), f))
    {
        int r = sscanf(
            line,
            " %63[^:]: "
            "%llu %llu %llu %llu %llu %llu %llu %llu "
            "%llu %llu %llu %llu %llu %llu %llu %llu",
            iface,
            &rx,
            &rx_packets,
            &rx_errors,
            &rx_drop,
            &rx_fifo,
            &rx_frame,
            &rx_compressed,
            &rx_multicast,
            &tx,
            &tx_packets,
            &tx_errors,
            &tx_drop,
            &tx_fifo,
            &tx_collisions,
            &tx_carrier,
            &tx_compressed
        );

        if (r != 17)
            continue;

        if (strcmp(iface, "lo") == 0)
            continue;

        if (receive)
            total += rx;
        else
            total += tx;
    }

    fclose(f);

    return total;
}


/* =========================================================
   HTTP
   ========================================================= */

static void send_response(
    int fd,
    const char *status,
    const char *content_type,
    const char *body
)
{
    dprintf(
        fd,
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        status,
        content_type,
        strlen(body),
        body
    );
}


static void send_json(
    int fd,
    const char *status,
    const char *body
)
{
    send_response(
        fd,
        status,
        "application/json",
        body
    );
}


/* =========================================================
   Session authentication
   ========================================================= */

static int valid_session(
    const char *request
)
{
    const char *cookie =
        strstr(request, "Cookie:");

    if (!cookie)
        cookie = strstr(request, "cookie:");

    if (!cookie)
        return 0;

    char expected[256];

    snprintf(
        expected,
        sizeof(expected),
        "pi_session=%s",
        SESSION_TOKEN
    );

    return strstr(cookie, expected) != NULL;
}


/* =========================================================
   URL decoding
   ========================================================= */

static void url_decode(
    char *dst,
    size_t dst_size,
    const char *src
)
{
    size_t out = 0;

    while (*src && out + 1 < dst_size)
    {
        if (*src == '%')
        {
            if (
                src[1] &&
                src[2]
            )
            {
                char hex[3];

                hex[0] = src[1];
                hex[1] = src[2];
                hex[2] = '\0';

                char *end;

                long value =
                    strtol(hex, &end, 16);

                if (*end == '\0')
                {
                    dst[out++] =
                        (char)value;

                    src += 3;
                    continue;
                }
            }
        }

        if (*src == '+')
            dst[out++] = ' ';
        else
            dst[out++] = *src;

        src++;
    }

    dst[out] = '\0';
}


/* =========================================================
   Login
   ========================================================= */

static void api_login(
    int fd,
    const char *request
)
{
    const char *body =
        strstr(request, "\r\n\r\n");

    if (!body)
    {
        send_json(
            fd,
            "400 Bad Request",
            "{\"ok\":false}"
        );

        return;
    }

    body += 4;

    char username_encoded[256] = {0};
    char password_encoded[512] = {0};

    const char *u =
        strstr(body, "username=");

    const char *p =
        strstr(body, "password=");

    if (u)
    {
        u += strlen("username=");

        size_t i = 0;

        while (
            u[i] &&
            u[i] != '&' &&
            i < sizeof(username_encoded) - 1
        )
        {
            username_encoded[i] = u[i];
            i++;
        }

        username_encoded[i] = '\0';
    }

    if (p)
    {
        p += strlen("password=");

        size_t i = 0;

        while (
            p[i] &&
            p[i] != '&' &&
            p[i] != '\r' &&
            p[i] != '\n' &&
            i < sizeof(password_encoded) - 1
        )
        {
            password_encoded[i] = p[i];
            i++;
        }

        password_encoded[i] = '\0';
    }

    char username[128];
    char password[256];

    url_decode(
        username,
        sizeof(username),
        username_encoded
    );

    url_decode(
        password,
        sizeof(password),
        password_encoded
    );

    if (
        strcmp(username, USERNAME) != 0 ||
        strcmp(password, PASSWORD) != 0
    )
    {
        send_json(
            fd,
            "401 Unauthorized",
            "{\"ok\":false,\"error\":\"Invalid username or password\"}"
        );

        return;
    }

    const char *reply =
        "{\"ok\":true}";

    dprintf(
        fd,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Cache-Control: no-store\r\n"
        "Set-Cookie: pi_session=%s; Path=/; HttpOnly; SameSite=Strict\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        SESSION_TOKEN,
        strlen(reply),
        reply
    );
}


/* =========================================================
   Status API
   ========================================================= */

static void api_status(
    int fd
)
{
    struct sysinfo info;

    if (sysinfo(&info) != 0)
    {
        send_json(
            fd,
            "500 Internal Server Error",
            "{\"error\":\"sysinfo failed\"}"
        );

        return;
    }

    unsigned long long ram_total =
        meminfo_value("MemTotal:");

    unsigned long long ram_available =
        meminfo_value("MemAvailable:");

    unsigned long long ram_used =
        ram_total > ram_available ?
        ram_total - ram_available :
        0;

    struct statvfs fs;

    unsigned long long disk_total = 0;
    unsigned long long disk_free = 0;

    if (statvfs("/", &fs) == 0)
    {
        disk_total =
            (unsigned long long)
            fs.f_blocks *
            fs.f_frsize;

        disk_free =
            (unsigned long long)
            fs.f_bavail *
            fs.f_frsize;
    }

    unsigned long long disk_used =
        disk_total > disk_free ?
        disk_total - disk_free :
        0;

    double ram_percent =
        ram_total ?
        100.0 *
        (double)ram_used /
        (double)ram_total :
        0;

    double disk_percent =
        disk_total ?
        100.0 *
        (double)disk_used /
        (double)disk_total :
        0;

    unsigned long long rx =
        network_bytes(1);

    unsigned long long tx =
        network_bytes(0);

    char body[4096];

    snprintf(
        body,
        sizeof(body),
        "{"
        "\"cpu\":%.2f,"
        "\"ram_total\":%llu,"
        "\"ram_used\":%llu,"
        "\"ram_percent\":%.2f,"
        "\"disk_total\":%llu,"
        "\"disk_used\":%llu,"
        "\"disk_percent\":%.2f,"
        "\"rx\":%llu,"
        "\"tx\":%llu,"
        "\"uptime\":%llu"
        "}",
        cpu_usage,
        ram_total,
        ram_used,
        ram_percent,
        disk_total,
        disk_used,
        disk_percent,
        rx,
        tx,
        (unsigned long long)
        info.uptime
    );

    send_json(
        fd,
        "200 OK",
        body
    );
}


/* =========================================================
   Processes API
   ========================================================= */

static void api_processes(
    int fd
)
{
    DIR *dir =
        opendir("/proc");

    if (!dir)
    {
        send_json(
            fd,
            "500 Internal Server Error",
            "{\"error\":\"cannot open /proc\"}"
        );

        return;
    }

    char *body =
        malloc(PROCESS_BUFFER);

    if (!body)
    {
        closedir(dir);

        send_json(
            fd,
            "500 Internal Server Error",
            "{\"error\":\"out of memory\"}"
        );

        return;
    }

    int pos = 0;
    int first = 1;

    body[pos++] = '[';

    struct dirent *entry;

    while (
        (entry = readdir(dir)) != NULL
    )
    {
        int pid =
            atoi(entry->d_name);

        if (pid <= 0)
            continue;

        char path[128];

        snprintf(
            path,
            sizeof(path),
            "/proc/%d/stat",
            pid
        );

        FILE *f =
            fopen(path, "r");

        if (!f)
            continue;

        char line[1024];

        if (!fgets(
                line,
                sizeof(line),
                f
            ))
        {
            fclose(f);
            continue;
        }

        fclose(f);

        char name[256] = "?";
        char state = '?';

        char *left =
            strchr(line, '(');

        char *right =
            strrchr(line, ')');

        if (
            left &&
            right &&
            right > left
        )
        {
            size_t len =
                (size_t)
                (right - left - 1);

            if (len >= sizeof(name))
                len = sizeof(name) - 1;

            memcpy(
                name,
                left + 1,
                len
            );

            name[len] = '\0';

            if (right[2])
                state = right[2];
        }

        int written =
            snprintf(
                body + pos,
                PROCESS_BUFFER - pos,
                "%s{\"pid\":%d,\"name\":\"%s\",\"state\":\"%c\"}",
                first ? "" : ",",
                pid,
                name,
                state
            );

        if (
            written < 0 ||
            pos + written >=
            PROCESS_BUFFER - 200
        )
            break;

        pos += written;
        first = 0;
    }

    closedir(dir);

    body[pos++] = ']';
    body[pos] = '\0';

    send_json(
        fd,
        "200 OK",
        body
    );

    free(body);
}


/* =========================================================
   Process control
   ========================================================= */

static void api_signal(
    int fd,
    const char *path
)
{
    const char *pid_text =
        strstr(path, "pid=");

    const char *sig_text =
        strstr(path, "sig=");

    if (!pid_text || !sig_text)
    {
        send_json(
            fd,
            "400 Bad Request",
            "{\"ok\":false,\"error\":\"Missing parameters\"}"
        );

        return;
    }

    int pid =
        atoi(pid_text + 4);

    int sig =
        atoi(sig_text + 4);

    /*
     * Only expose:
     *
     * SIGSTOP  = 19
     * SIGCONT  = 18
     * SIGTERM  = 15
     */

    if (
        sig != SIGSTOP &&
        sig != SIGCONT &&
        sig != SIGTERM
    )
    {
        send_json(
            fd,
            "400 Bad Request",
            "{\"ok\":false,\"error\":\"Signal not allowed\"}"
        );

        return;
    }

    /*
     * Protect PID 1 and ourselves.
     */

    if (
        pid <= 1 ||
        pid == getpid()
    )
    {
        send_json(
            fd,
            "403 Forbidden",
            "{\"ok\":false,\"error\":\"Protected process\"}"
        );

        return;
    }

    if (kill(pid, sig) != 0)
    {
        char body[256];

        snprintf(
            body,
            sizeof(body),
            "{\"ok\":false,\"error\":\"%s\"}",
            strerror(errno)
        );

        send_json(
            fd,
            "400 Bad Request",
            body
        );

        return;
    }

    send_json(
        fd,
        "200 OK",
        "{\"ok\":true}"
    );
}


/* =========================================================
   Ports API
   ========================================================= */

static void api_ports(
    int fd
)
{
    char *body =
        malloc(PORT_BUFFER);

    if (!body)
    {
        send_json(
            fd,
            "500 Internal Server Error",
            "{\"error\":\"out of memory\"}"
        );

        return;
    }

    int pos = 0;
    int first = 1;

    body[pos++] = '[';

    const char *files[] =
    {
        "/proc/net/tcp",
        "/proc/net/tcp6",
        "/proc/net/udp",
        "/proc/net/udp6"
    };

    for (int file = 0; file < 4; file++)
    {
        FILE *f =
            fopen(files[file], "r");

        if (!f)
            continue;

        char line[512];

        fgets(
            line,
            sizeof(line),
            f
        );

        while (
            fgets(
                line,
                sizeof(line),
                f
            )
        )
        {
            char local[128];
            char remote[128];
            unsigned int state;

            int r =
                sscanf(
                    line,
                    " %*d: %127s %127s %x",
                    local,
                    remote,
                    &state
                );

            if (r != 3)
                continue;

            /*
             * TCP LISTEN state = 0A.
             */

            if (
                file < 2 &&
                state != 0x0A
            )
                continue;

            char *colon =
                strrchr(local, ':');

            if (!colon)
                continue;

            unsigned int port =
                strtoul(
                    colon + 1,
                    NULL,
                    16
                );

            const char *protocol =
                file < 2 ?
                "TCP" :
                "UDP";

            int written =
                snprintf(
                    body + pos,
                    PORT_BUFFER - pos,
                    "%s{\"proto\":\"%s\",\"port\":%u}",
                    first ? "" : ",",
                    protocol,
                    port
                );

            if (
                written < 0 ||
                pos + written >=
                PORT_BUFFER - 200
            )
                break;

            pos += written;
            first = 0;
        }

        fclose(f);
    }

    body[pos++] = ']';
    body[pos] = '\0';

    send_json(
        fd,
        "200 OK",
        body
    );

    free(body);
}


/* =========================================================
   HTML
   ========================================================= */

static const char html[] =
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<meta name='theme-color' content='#0b1016'>"
"<title>Pi Monitor</title>"

"<style>"

"*{box-sizing:border-box}"

"body{"
"margin:0;"
"background:#080c11;"
"color:#e7edf4;"
"font-family:system-ui,-apple-system,BlinkMacSystemFont,sans-serif"
"}"

"header{"
"height:64px;"
"display:flex;"
"align-items:center;"
"justify-content:space-between;"
"padding:0 20px;"
"background:#10161e;"
"border-bottom:1px solid #26303b"
"}"

"h1{"
"font-size:20px;"
"margin:0;"
"}"

".online{"
"font-size:12px;"
"color:#62e49b"
"}"

"nav{"
"display:flex;"
"gap:7px;"
"padding:12px 16px;"
"background:#0e131a;"
"border-bottom:1px solid #26303b;"
"overflow-x:auto"
"}"

"nav button{"
"white-space:nowrap;"
"border:1px solid #344152;"
"background:#151d27;"
"color:#dce5ee;"
"padding:9px 14px;"
"border-radius:8px;"
"cursor:pointer"
"}"

"nav button.active{"
"background:#27374b;"
"border-color:#4b6582"
"}"

"main{"
"max-width:1250px;"
"margin:auto;"
"padding:18px"
"}"

".tab{"
"display:none"
"}"

".tab.visible{"
"display:block"
"}"

".grid{"
"display:grid;"
"grid-template-columns:repeat(auto-fit,minmax(190px,1fr));"
"gap:12px"
"}"

".card{"
"background:#10161e;"
"border:1px solid #26313d;"
"border-radius:12px;"
"padding:16px;"
"margin-bottom:12px"
"}"

".label{"
"font-size:12px;"
"color:#8290a1"
"}"

".value{"
"font-size:25px;"
"font-weight:650;"
"margin-top:5px"
"}"

".bar{"
"height:7px;"
"background:#202a35;"
"border-radius:5px;"
"margin-top:12px;"
"overflow:hidden"
"}"

".fill{"
"height:100%;"
"width:0%;"
"background:#5fa8ff;"
"border-radius:5px;"
"transition:width .3s"
"}"

"table{"
"width:100%;"
"border-collapse:collapse"
"}"

"th,td{"
"text-align:left;"
"padding:10px;"
"border-bottom:1px solid #222c37"
"}"

"th{"
"font-size:12px;"
"color:#8492a3"
"}"

"button{"
"border:1px solid #344152;"
"background:#151d27;"
"color:#e8edf3;"
"padding:7px 10px;"
"border-radius:7px;"
"cursor:pointer"
"}"

"button:hover{"
"background:#222e3d"
"}"

".danger{"
"background:#381a1e;"
"border-color:#71313a;"
"color:#ff9ca5"
"}"

".danger:hover{"
"background:#4a2026"
"}"

".login{"
"position:fixed;"
"inset:0;"
"display:flex;"
"align-items:center;"
"justify-content:center;"
"background:#080c11"
"}"

".loginbox{"
"width:330px;"
"padding:25px;"
"background:#111820;"
"border:1px solid #293544;"
"border-radius:14px;"
"box-shadow:0 20px 60px rgba(0,0,0,.4)"
"}"

".loginbox h2{"
"margin:0 0 6px 0"
"}"

".loginbox p{"
"color:#8491a1;"
"font-size:13px"
"}"

"input{"
"width:100%;"
"padding:11px;"
"margin:5px 0;"
"border-radius:8px;"
"border:1px solid #303c4b;"
"background:#090e14;"
"color:white;"
"outline:none"
"}"

"input:focus{"
"border-color:#5b8bc1"
"}"

".loginbox button{"
"width:100%;"
"margin-top:8px;"
"padding:11px"
"}"

".error{"
"color:#ff818b;"
"font-size:13px;"
"margin-top:10px"
"}"

".muted{"
"color:#8290a1;"
"font-size:13px"
"}"

"@media(max-width:600px){"
"main{padding:12px}"
"th,td{padding:8px;font-size:12px}"
".value{font-size:22px}"
"}"

"</style>"
"</head>"

"<body>"

"<div id='login' class='login'>"

"<div class='loginbox'>"

"<h2>Pi Monitor</h2>"

"<p>Administrator login</p>"

"<input"
"id='username'"
"placeholder='Username'"
"autocomplete='username'>"

"<input"
"id='password'"
"type='password'"
"placeholder='Password'"
"autocomplete='current-password'>"

"<button onclick='login()'>Sign in</button>"

"<div id='error' class='error'></div>"

"</div>"

"</div>"


"<div id='app' style='display:none'>"

"<header>"
"<h1>Pi Monitor</h1>"
"<span class='online'>● ONLINE</span>"
"</header>"

"<nav>"

"<button"
"class='active'"
"onclick=\"tab('overview',this)\">"
"Overview"
"</button>"

"<button"
"onclick=\"tab('processes',this)\">"
"Processes"
"</button>"

"<button"
"onclick=\"tab('ports',this)\">"
"Ports"
"</button>"

"<button"
"onclick=\"tab('network',this)\">"
"Network"
"</button>"

"</nav>"

"<main>"

"<section id='overview' class='tab visible'>"

"<div class='grid'>"

"<div class='card'>"
"<div class='label'>CPU Usage</div>"
"<div class='value' id='cpu'>--</div>"
"<div class='bar'>"
"<div id='cpuBar' class='fill'></div>"
"</div>"
"</div>"

"<div class='card'>"
"<div class='label'>Memory</div>"
"<div class='value' id='ram'>--</div>"
"<div class='bar'>"
"<div id='ramBar' class='fill'></div>"
"</div>"
"</div>"

"<div class='card'>"
"<div class='label'>Disk</div>"
"<div class='value' id='disk'>--</div>"
"<div class='bar'>"
"<div id='diskBar' class='fill'></div>"
"</div>"
"</div>"

"<div class='card'>"
"<div class='label'>Uptime</div>"
"<div class='value' id='uptime'>--</div>"
"</div>"

"</div>"

"</section>"


"<section id='processes' class='tab'>"

"<div class='card'>"

"<h3>Processes</h3>"

"<div id='processList'>"
"Loading..."
"</div>"

"</div>"

"</section>"


"<section id='ports' class='tab'>"

"<div class='card'>"

"<h3>Open Ports</h3>"

"<p class='muted'>"
"Listening TCP and active UDP ports"
"</p>"

"<div id='portList'>"
"Loading..."
"</div>"

"</div>"

"</section>"


"<section id='network' class='tab'>"

"<div class='grid'>"

"<div class='card'>"
"<div class='label'>Total Received</div>"
"<div class='value' id='rx'>--</div>"
"</div>"

"<div class='card'>"
"<div class='label'>Total Transmitted</div>"
"<div class='value' id='tx'>--</div>"
"</div>"

"</div>"

"</section>"

"</main>"

"</div>"


"<script>"

"let refreshing=false;"

"function login(){"

"let username="
"document.getElementById('username').value;"

"let password="
"document.getElementById('password').value;"

"let body="
"'username='+encodeURIComponent(username)+"
"'&password='+encodeURIComponent(password);"

"fetch('/api/login',{"
"method:'POST',"
"headers:{"
"'Content-Type':"
"'application/x-www-form-urlencoded'"
"},"
"body:body"
"})"

".then(r=>r.json())"

".then(data=>{"

"if(!data.ok)"
"throw new Error('login');"

"document.getElementById('login').style.display='none';"

"document.getElementById('app').style.display='block';"

"refresh();"

"})"

".catch(()=>{"

"document.getElementById('error').textContent="
"'Invalid username or password';"

"});"

"}"


"document.getElementById('password').addEventListener("
"'keydown',function(e){"
"if(e.key==='Enter')login();"
"});"


"function api(url,options={}){"

"return fetch(url,options)"
".then(response=>{"

"if(response.status===401){"
"location.reload();"
"throw new Error('unauthorized');"
"}"

"return response.json();"

"});"

"}"


"function tab(id,button){"

"document.querySelectorAll('.tab')"
".forEach(x=>x.classList.remove('visible'));"

"document.getElementById(id)"
".classList.add('visible');"

"document.querySelectorAll('nav button')"
".forEach(x=>x.classList.remove('active'));"

"button.classList.add('active');"

"if(id==='processes')"
"loadProcesses();"

"if(id==='ports')"
"loadPorts();"

"}"


"function formatBytes(bytes){"

"if(bytes<1024)"
"return bytes+' B';"

"if(bytes<1048576)"
"return (bytes/1024).toFixed(1)+' KB';"

"if(bytes<1073741824)"
"return (bytes/1048576).toFixed(1)+' MB';"

"return (bytes/1073741824).toFixed(2)+' GB';"

"}"


"function refresh(){"

"if(refreshing)return;"

"refreshing=true;"

"api('/api/status')"

".then(x=>{"

"document.getElementById('cpu').textContent="
"x.cpu.toFixed(1)+'%';"

"document.getElementById('cpuBar').style.width="
"x.cpu+'%';"

"document.getElementById('ram').textContent="
"formatBytes(x.ram_used)+' / '+formatBytes(x.ram_total);"

"document.getElementById('ramBar').style.width="
"x.ram_percent+'%';"

"document.getElementById('disk').textContent="
"x.disk_percent.toFixed(1)+'%';"

"document.getElementById('diskBar').style.width="
"x.disk_percent+'%';"

"let seconds=x.uptime;"

"let days=Math.floor(seconds/86400);"

"seconds%=86400;"

"let hours=Math.floor(seconds/3600);"

"seconds%=3600;"

"let minutes=Math.floor(seconds/60);"

"document.getElementById('uptime').textContent="
"days+'d '+hours+'h '+minutes+'m';"

"document.getElementById('rx').textContent="
"formatBytes(x.rx);"

"document.getElementById('tx').textContent="
"formatBytes(x.tx);"

"})"

".catch(()=>{})"

".finally(()=>{"

"refreshing=false;"

"});"

"}"


"function loadProcesses(){"

"api('/api/processes')"

".then(processes=>{"

"let html="
"'<table><tr>"
"<th>PID</th>"
"<th>Process</th>"
"<th>State</th>"
"<th>Actions</th>"
"</tr>';"

"processes.forEach(p=>{"

"html+="
"'<tr>'"
"'<td>'+p.pid+'</td>'"
"'<td>'+escapeHtml(p.name)+'</td>'"
"'<td>'+p.state+'</td>'"
"'<td>'"

"'<button onclick=\"processControl('+"
"p.pid+',19)\">Stop</button> '"

"'<button onclick=\"processControl('+"
"p.pid+',18)\">Resume</button> '"

"'<button class=\"danger\" "
"onclick=\"processControl('+"
"p.pid+',15)\">Terminate</button>'"

"'</td>'"
"'</tr>';"

"});"

"html+='</table>';"

"document.getElementById('processList').innerHTML=html;"

"});"

"}"


"function processControl(pid,signal){"

"let text="

"signal===19?"
"'Stop process '+pid+'?' :"

"signal===18?"
"'Resume process '+pid+'?' :"

"'Terminate process '+pid+'?';"

"if(!confirm(text))return;"

"api('/api/signal?pid='+pid+'&sig='+signal,{"
"method:'POST'"
"})"

".then(()=>loadProcesses());"

"}"


"function loadPorts(){"

"api('/api/ports')"

".then(ports=>{"

"let html="
"'<table><tr>"
"<th>Protocol</th>"
"<th>Port</th>"
"</tr>';"

"ports.forEach(p=>{"

"html+="
"'<tr>'"
"'<td>'+p.proto+'</td>'"
"'<td>'+p.port+'</td>'"
"'</tr>';"

"});"

"html+='</table>';"

"document.getElementById('portList').innerHTML=html;"

"});"

"}"


"function escapeHtml(value){"

"return String(value)"
".replaceAll('&','&amp;')"
".replaceAll('<','&lt;')"
".replaceAll('>','&gt;')"
".replaceAll('\"','&quot;')"
".replaceAll(\"'\",'&#039;');"

"}"


"setInterval(refresh,3000);"

"</script>"

"</body>"
"</html>";


/* =========================================================
   Serve HTML
   ========================================================= */

static void serve_html(
    int fd
)
{
    send_response(
        fd,
        "200 OK",
        "text/html; charset=utf-8",
        html
    );
}


/* =========================================================
   HTTP request handler
   ========================================================= */

static void handle_client(int fd)
{
    char request[REQUEST_SIZE];

    ssize_t received =
        recv(fd, request, sizeof(request) - 1, 0);

    if (received <= 0)
        return;

    request[received] = '\0';

    char method[16];
    char path[2048];

    if (sscanf(request, "%15s %2047s", method, path) != 2)
    {
        send_response(
            fd,
            "400 Bad Request",
            "text/plain",
            "Bad request"
        );
        return;
    }

    /*
     * IMPORTANT:
     * The main HTML page is public.
     * This is what displays the HTML login screen.
     */
    if (strcmp(path, "/") == 0 &&
        strcmp(method, "GET") == 0)
    {
        serve_html(fd);
        return;
    }

    /*
     * Login is also public.
     * The browser submits the HTML form here.
     */
    if (strcmp(path, "/api/login") == 0 &&
        strcmp(method, "POST") == 0)
    {
        api_login(fd, request);
        return;
    }

    /*
     * Everything below this point requires
     * our cookie-based HTML login session.
     */
    if (!valid_session(request))
    {
        /*
         * DO NOT send WWW-Authenticate.
         * Otherwise Chrome displays its own
         * username/password popup.
         */
        send_json(
            fd,
            "401 Unauthorized",
            "{\"error\":\"login required\"}"
        );
        return;
    }

    if (strcmp(path, "/api/status") == 0)
    {
        update_cpu();
        api_status(fd);
        return;
    }

    if (strcmp(path, "/api/processes") == 0)
    {
        api_processes(fd);
        return;
    }

    if (strcmp(path, "/api/ports") == 0)
    {
        api_ports(fd);
        return;
    }

    if (strncmp(path, "/api/signal?", 12) == 0 &&
        strcmp(method, "POST") == 0)
    {
        api_signal(fd, path);
        return;
    }

    send_json(
        fd,
        "404 Not Found",
        "{\"error\":\"not found\"}"
    );
}


/* =========================================================
   Main
   ========================================================= */

int main(void)
{
    /*
     * Initial CPU measurement.
     */

    previous_total =
        read_cpu_total();

    previous_idle =
        read_cpu_idle();


    int server =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (server < 0)
    {
        perror("socket");
        return 1;
    }


    int reuse = 1;

    setsockopt(
        server,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );


    struct sockaddr_in address;

    memset(
        &address,
        0,
        sizeof(address)
    );

    address.sin_family =
        AF_INET;

    address.sin_addr.s_addr =
        htonl(INADDR_ANY);

    address.sin_port =
        htons(PORT);


    if (
        bind(
            server,
            (struct sockaddr *)&address,
            sizeof(address)
        ) < 0
    )
    {
        perror("bind");
        close(server);

        return 1;
    }


    if (
        listen(
            server,
            8
        ) < 0
    )
    {
        perror("listen");
        close(server);

        return 1;
    }


    printf(
        "Pi Monitor listening on port %d\n",
        PORT
    );

    fflush(stdout);


    while (1)
    {
        int client =
            accept(
                server,
                NULL,
                NULL
            );

        if (client < 0)
        {
            if (errno == EINTR)
                continue;

            continue;
        }


        handle_client(client);

        close(client);
    }


    close(server);

    return 0;
}
