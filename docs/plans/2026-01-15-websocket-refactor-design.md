# Widgeteer WebSocket Refactor Design

**Date:** 2026-01-15
**Status:** Approved

## Overview

Refactor Widgeteer from HTTP to WebSocket-only communication, adding authentication, recording, extended widget support, unit tests, and a CLI tool.

## Decisions Made

| Decision | Choice |
|----------|--------|
| Protocol | WebSocket only (no HTTP) |
| Authentication | API key in connection URL (`ws://host:port?token=key`) |
| Recording format | JSON test file format (same as sample_tests.json) |
| CLI language | Python (extends existing widgeteer_client.py) |
| Test framework | Qt Test |
| Highlighter | Deferred to future iteration |

---

## 1. WebSocket Protocol

### Connection

```
ws://localhost:9000                     # No auth
ws://localhost:9000?token=your-api-key  # With auth
```

If `setApiKey()` was called, connections without valid token are closed with code 4001.

### Message Format

**Client → Server:**
```json
{
  "id": "unique-request-id",
  "type": "command",
  "command": "click",
  "params": {"target": "@name:button"}
}
```

**Server → Client:**
```json
{
  "id": "unique-request-id",
  "type": "response",
  "success": true,
  "result": {"clicked": true}
}
```

### Message Types

| Type | Direction | Purpose |
|------|-----------|---------|
| `command` | Client→Server | Execute a command |
| `response` | Server→Client | Command result |
| `event` | Server→Client | Real-time events |
| `subscribe` | Client→Server | Subscribe to events |
| `unsubscribe` | Client→Server | Unsubscribe from events |
| `record_start` | Client→Server | Start recording |
| `record_stop` | Client→Server | Stop recording, get JSON |

---

## 2. Server API Changes

```cpp
class Server : public QObject {
public:
  // Existing
  bool start(quint16 port = 9000);
  void stop();

  // Authentication
  void setApiKey(const QString& key);
  QString apiKey() const;

  // Event broadcasting
  void enableEventBroadcast(bool enable);

  // Recording
  void startRecording();
  void stopRecording();
  QJsonObject getRecording();
  bool isRecording() const;

signals:
  void clientConnected(const QString& clientId);
  void clientDisconnected(const QString& clientId);
  void clientAuthenticated(const QString& clientId);
  void authenticationFailed(const QString& clientId);
};
```

---

## 3. Extended Widget Support in set_value

| Widget | Value Type | Behavior |
|--------|------------|----------|
| `QDateEdit` | string `"2024-01-15"` | Sets date |
| `QTimeEdit` | string `"14:30:00"` | Sets time |
| `QDateTimeEdit` | string ISO format | Sets datetime |
| `QDial` | int | Sets dial value |
| `QProgressBar` | int | Sets progress |
| `QTableWidget` | `{"row": r, "col": c, "value": v}` | Sets cell |
| `QTreeWidget` | `{"path": [0,1,2], "value": v}` | Sets item |
| `QListWidget` | `{"row": r, "value": v}` | Sets item text |
| `QPlainTextEdit` | string | Sets text |
| `QLabel` | string | Sets label text |
| `QGroupBox` | bool | Sets checked (if checkable) |
| `QCalendarWidget` | string `"2024-01-15"` | Sets date |
| `QStackedWidget` | int | Sets current index |

---

## 4. Recording Flow

1. Client sends `record_start`
2. User interacts with app
3. Client sends `record_stop`
4. Server returns JSON (sample_tests.json format)
5. Client saves to file

---

## 5. Unit Tests

```
tests/
├── unit/
│   ├── tst_elementfinder.cpp
│   ├── tst_uiintrospector.cpp
│   ├── tst_protocol.cpp
│   ├── tst_commandexecutor.cpp
│   └── tst_synchronizer.cpp
├── integration/
│   └── (existing tests)
└── CMakeLists.txt
```

---

## 6. Python CLI Tool

```bash
widgeteer --port 9000 --token secret click "@name:button"
widgeteer tree --depth 2
widgeteer record --output test.json
widgeteer run test.json
```

---

## Future Work (Deferred)

- **Visual element highlighter** - Overlay widget showing matched elements. Implementation planned: `HighlightOverlay` class with colored border, auto-hide after duration, tracks target movement.
