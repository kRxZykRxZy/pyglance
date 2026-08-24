CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Wshadow -Wformat=2 -D_GNU_SOURCE
LDFLAGS ?= -s
TARGET := pi-monitor
SOURCES := src/main.c src/http.c src/system.c src/cpu.c src/firewall.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean install
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)
	install -d /usr/share/pi-monitor/web
	install -Dm644 web/login.html /usr/share/pi-monitor/web/login.html
	install -Dm644 web/dashboard.html /usr/share/pi-monitor/web/dashboard.html
	install -Dm644 web/app.css /usr/share/pi-monitor/web/app.css
	install -Dm644 web/app.js /usr/share/pi-monitor/web/app.js
	install -Dm644 pi-monitor.service /etc/systemd/system/pi-monitor.service
	systemctl daemon-reload
