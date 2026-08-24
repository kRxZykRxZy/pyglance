# PyGlance Dream Monitor — 50 fully implemented feature specifications

This document is the implementation contract for the next major control-plane generation. Every feature is designed for a Raspberry Pi 2-class agent with a strict normal operating target of <=10% CPU and <=100 MB RSS. Fast UI refreshes read cached telemetry; expensive collectors are scheduled independently.

## CPU and thermal
1. Per-core utilization: calculate deltas from `/proc/stat` and expose every logical CPU.
2. CPU frequency history: sample cpufreq current/max values and retain a bounded ring buffer.
3. Governor inspection/control: read the active governor and expose an authenticated allow-list control.
4. Throttle detection: inspect thermal/throttle indicators and record transitions.
5. Thermal zones: enumerate `/sys/class/thermal/thermal_zone*`, labels and temperatures.
6. Load history: maintain 1/5/15-minute load history with bounded storage.

## Memory
7. RAM breakdown: MemTotal, MemFree, Buffers, Cached, SReclaimable, Available, Swap.
8. Swap monitoring: total/free/used and swap-in/out counters.
9. Memory pressure: PSI when available, with graceful fallback on older kernels.
10. OOM detection: inspect kernel log/event sources for recent OOM kills.

## Storage
11. Disk I/O: per-device read/write bytes and operations using `/proc/diskstats`.
12. Disk health: expose safe read-only health information where available.
13. Filesystem usage: capacity and free space for mounted filesystems.
14. Inode usage: inode totals/free/used percentages.
15. Mount management: authenticated mount inventory with safe read-only details first.

## Network
16. Interface bandwidth: RX/TX rates from interface counters.
17. Packet health: errors and drops per interface.
18. Connection table: bounded socket summary without spawning expensive commands each poll.
19. DNS configuration: resolver inventory and reachability checks on demand.
20. Routing table: parsed route inventory and default route health.
21. Firewall inspection: detect nftables/iptables/UFW and expose backend, state, counters and rules.
22. Firewall operations: authenticated, explicit, confirmation-gated rule operations with recovery protection.

## Services and processes
23. Service dependency graph: parse systemd metadata on slow refresh and cache it.
24. Service lifecycle: authenticated start/stop/restart for an explicit service allow-list.
25. Failed-service detection: monitor failed units and transitions.
26. Process CPU ranking: bounded top-N process list using `/proc`.
27. Process RAM ranking: bounded top-N process list using `/proc`.
28. Process lifecycle events: detect new/exited processes without retaining unbounded history.

## Logs and events
29. Kernel logs: bounded recent kernel log reader.
30. Service logs: bounded service journal reader with pagination/limits.
31. Event timeline: normalized health/service/security events with bounded retention.

## Alerting
32. Threshold engine: configurable CPU/RAM/temp/disk/network/service thresholds.
33. Alert acknowledgement: acknowledge/resolve alerts and retain state.
34. Alert history: bounded event history with severity and timestamps.
35. Maintenance mode: suppress selected alerts during planned operations.

## Security
36. Host security status: auth/session/security-header and local exposure checks.
37. Login/session audit: successful/failed login events and active sessions.
38. Configuration snapshots: bounded, redacted configuration snapshots.
39. Configuration diff: compare snapshots and highlight changed settings.
40. Scheduled tasks: inspect systemd timers/cron metadata and report failures.

## Inventory and intelligence
41. Host inventory: hostname, OS, kernel, architecture, boot ID, machine ID hash/redaction.
42. OS/kernel inventory: release, kernel, command-line and feature availability.
43. Hardware inventory: CPU, memory, board/model and device metadata.
44. Health score: deterministic weighted health score from observable signals.
45. Capacity forecast: simple bounded trend estimates from retained telemetry, clearly labelled estimates.
46. Resource budgets: configure/visualize CPU and memory targets and headroom.

## Platform operations
47. API health: endpoint latency/error counters and agent health.
48. Agent self-diagnostics: dependency, permissions, filesystem and collector checks.
49. Automatic recovery: safe recovery actions for configured service failures with cooldowns.
50. Full operations dashboard: unified health, alerts, capacity, services, storage, network and security overview.

## Engineering constraints
- No shell execution in fast telemetry paths.
- No unbounded arrays, log buffers, process lists or filesystem results.
- 500 ms UI refresh is allowed to consume cached telemetry; collectors use independent schedules.
- Filesystem and log APIs require pagination/limits.
- Destructive operations require explicit authentication and confirmation.
- Firewall operations require a rollback-safe mechanism; LAN-only is not treated as sufficient security.
- All numeric telemetry must reject NaN/Infinity before JSON serialization or charting.
