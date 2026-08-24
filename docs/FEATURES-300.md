# PyGlance — 300 Feature Production Roadmap

This roadmap is the target feature set for the production/industrial edition. Features are grouped so implementation can be delivered incrementally without making the Raspberry Pi 2B agent heavy.

## 1. Core telemetry
1. CPU utilization
2. Per-core CPU utilization
3. CPU load average
4. CPU frequency
5. CPU maximum frequency
6. CPU minimum frequency
7. CPU governor
8. CPU temperature
9. Thermal-zone discovery
10. Thermal throttling detection
11. Undervoltage detection
12. ARM voltage telemetry where supported
13. RAM total
14. RAM used
15. RAM available
16. RAM cached
17. Swap usage
18. Swap-in/out counters
19. Root filesystem usage
20. Filesystem free space
21. Filesystem inode usage
22. Disk I/O throughput
23. Disk I/O operations
24. Disk I/O latency
25. Network RX bytes
26. Network TX bytes
27. Network packet counts
28. Network errors
29. Network drops
30. Interface link state

## 2. Historical monitoring
31. 500 ms live sampling
32. One-minute rollups
33. Five-minute rollups
34. Fifteen-minute rollups
35. Hourly rollups
36. Daily rollups
37. Weekly rollups
38. Monthly rollups
39. Configurable retention
40. Local metric storage
41. Circular metric storage
42. Metric compression
43. Metric integrity checks
44. Historical CPU charts
45. Historical RAM charts
46. Historical temperature charts
47. Historical disk charts
48. Historical network charts
49. Historical load charts
50. Historical I/O charts

## 3. Charts and visualization
51. Responsive charts
52. Mobile charts
53. Desktop charts
54. Stable axis scaling
55. NaN-safe rendering
56. Infinity-safe rendering
57. Empty-state rendering
58. Sensor-unavailable rendering
59. Chart legends
60. Chart tooltips
61. Time-range selector
62. Zoom controls
63. Pan controls
64. Reset zoom
65. Export chart data
66. PNG chart export
67. CSV metric export
68. Threshold overlays
69. Alert markers
70. Event markers

## 4. Device information
71. Hostname
72. Hardware model
73. Board revision
74. Serial number masking
75. OS release
76. Kernel version
77. Architecture
78. CPU model
79. CPU core count
80. Total uptime
81. Boot time
82. Last reboot reason
83. Firmware information
84. Raspberry Pi firmware status
85. Clock synchronization status
86. Timezone
87. Current system time
88. NTP status
89. Agent version
90. Dashboard version

## 5. Storage and filesystem
91. Block-device inventory
92. Partition inventory
93. Filesystem inventory
94. Mount-point inventory
95. Filesystem labels
96. Filesystem UUIDs
97. Disk health status
98. Read-only detection
99. Removable-device detection
100. USB-storage detection
101. Filesystem browser
102. Directory navigation
103. Parent-directory navigation
104. File metadata
105. File size display
106. File permissions display
107. File owner display
108. File timestamps
109. Safe text-file viewer
110. File search
111. Directory size calculation
112. Disk-usage tree
113. Storage threshold alerts
114. Read-only filesystem alerts
115. Mount failure alerts

## 6. Process management
116. Process inventory
117. PID lookup
118. Process state
119. CPU percentage per process
120. RAM percentage per process
121. Process command line
122. Process owner
123. Process start time
124. Process uptime
125. Process tree
126. Parent PID
127. Child processes
128. Process search
129. Process sorting
130. Process filtering
131. Graceful terminate
132. Force terminate
133. Stop process
134. Resume process
135. Process action audit

## 7. Services
136. Systemd service inventory
137. Running-service detection
138. Failed-service detection
139. Service restart
140. Service stop
141. Service start
142. Service enable status
143. Service disable status
144. Service uptime
145. Service failure count
146. Service logs
147. Service health checks
148. Service dependency view
149. Critical-service alerts
150. PiGlance self-health

## 8. Logs and events
151. System journal viewer
152. PiGlance journal viewer
153. Kernel log viewer
154. Authentication log viewer
155. Boot log viewer
156. Log severity filtering
157. Log search
158. Log timestamp filtering
159. Log source filtering
160. Live log streaming
161. Log download
162. Log retention controls
163. Event timeline
164. Warning events
165. Critical events

## 9. Alerting
166. CPU threshold alerts
167. RAM threshold alerts
168. Disk threshold alerts
169. Temperature threshold alerts
170. Load threshold alerts
171. Network-down alerts
172. Packet-error alerts
173. Filesystem alerts
174. Service-failure alerts
175. Agent-down alerts
176. Undervoltage alerts
177. Thermal-throttle alerts
178. Alert hysteresis
179. Alert cooldown
180. Alert acknowledgement
181. Alert escalation
182. Alert severity levels
183. Alert history
184. Alert suppression
185. Maintenance windows

## 10. Notifications
186. Browser notifications
187. In-dashboard notifications
188. Email notifications
189. Webhook notifications
190. Generic HTTP callbacks
191. Notification retry
192. Notification backoff
193. Notification templates
194. Notification routing
195. Per-alert notification rules
196. Test notification
197. Notification delivery logs
198. Notification failure alerts
199. Quiet hours
200. Notification preferences

## 11. Authentication and security
201. Password authentication
202. Password hashing
203. Random session tokens
204. Session expiration
205. Session revocation
206. Logout invalidation
207. Login rate limiting
208. Brute-force backoff
209. CSRF protection
210. Security headers
211. SameSite cookies
212. HttpOnly cookies
213. Secure cookies under TLS
214. Password change
215. Forced password rotation
216. Read-only role
217. Operator role
218. Administrator role
219. Permission checks
220. Audit authentication events

## 12. Enterprise access
221. HTTPS/TLS
222. Certificate configuration
223. Certificate expiry warning
224. Reverse-proxy support
225. Trusted proxy configuration
226. IP allowlists
227. IP denylists
228. API tokens
229. Token revocation
230. Scoped API permissions
231. Session/device list
232. Remote logout
233. Login history
234. Failed-login history
235. Security-event dashboard

## 13. Fleet management
236. Device registration
237. Device identity
238. Device groups
239. Device tags
240. Device search
241. Device filtering
242. Device health state
243. Device offline detection
244. Fleet overview
245. Fleet CPU overview
246. Fleet RAM overview
247. Fleet temperature overview
248. Fleet disk overview
249. Fleet network overview
250. Fleet alerts

## 14. Remote operations
251. Remote reboot
252. Remote shutdown
253. Service restart
254. Maintenance mode
255. Configuration push
256. Agent update
257. Version reporting
258. Update status
259. Rollback support
260. Operation queue
261. Operation progress
262. Operation timeout
263. Operation retry
264. Operation audit
265. Safe command allowlist

## 15. Industrial/operations features
266. Site management
267. Rack/location metadata
268. Asset identifiers
269. Equipment notes
270. Maintenance schedules
271. Maintenance history
272. Incident records
273. Incident acknowledgement
274. Incident assignment
275. Operational runbooks
276. Health score
277. Availability percentage
278. SLA tracking
279. Downtime tracking
280. Capacity planning

## 16. API and platform
281. Versioned REST API
282. API health endpoint
283. API status endpoint
284. OpenAPI specification
285. Request IDs
286. Structured error responses
287. API rate limiting
288. API audit logging
289. Metrics endpoint
290. Agent heartbeat protocol
291. Agent registration protocol
292. Fleet synchronization
293. Configuration schema versioning
294. Database migration system
295. Backup/export system
296. Configuration import/export
297. Health/self-test endpoint
298. Automated regression tests
299. Security test suite
300. Production deployment checklist

## Implementation order

### Phase A — reliability
- Chart renderer hardening
- 500 ms polling with overlap protection
- API error handling
- authentication enforcement
- session lifecycle
- sensor/error handling

### Phase B — observability
- persistent metrics
- historical charts
- event store
- alert engine
- notification engine

### Phase C — operations
- filesystem browser/viewer
- service manager
- audit log
- maintenance mode
- safe remote operations

### Phase D — security
- password hashing
- RBAC
- CSRF
- rate limiting
- HTTPS/TLS
- API tokens

### Phase E — fleet
- agent protocol
- device registration
- fleet dashboard
- centralized alerts
- remote updates

### Design constraint
The Pi agent must remain lightweight enough for low-power Raspberry Pi hardware. Expensive historical aggregation, fleet analytics and long-term storage should be optional and preferably run on a separate controller/server.
