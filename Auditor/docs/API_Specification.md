# Endpoints
  1. `/audit/events/ POST`
  2. `/audit/events/ GET`

Protocol: **`HTTP`**: support of versions `HTTP1.0`, `HTTP1.1` -- **required**, versions `HTTP2`, `HTTP3/QUIC` -- **optional**

*The rest* `/audit/` *endpoints are private and defined by developer for his purposes*

## **`/audit/events/ POST`**
```json
POST: /audit/events/ HTTP/1.1 
Content-Type: application/json      # required
...                                 # optional
{
  "timestamp": str,              // required
  "user": str,                   // required
  "component": str,              // optional
  "op": str,                     // required
  "session_id": int,             // optional
  "req_id": int,                 // optional
  "res": json-object,            // optional, user defined. It may be any valid json object
  "attributes": json-object      // optional, user defined. Must not contain json objects and arrays as fields 
}
```

## **`/audit/events/ GET`**
```
GET: /audit/events/query?<key>:<value>[,<value>][&<key>:<value>[,<value>]]
```
Get returns the same JSON as user provided with POST request.

Common keys:
  1. `ev_ts_start`: str -- sets the beginning of the filter interval by timestampts
  2. `ev_ts_end`: str -- sets the end of the filter interval by timestampts
  3. `ev_ts`: str -- sets the exact timestamp to search. *Can not be used with `ev_ts_start` and `ev_ts_end`*
  4. `ev_component`: list[str] -- filter logs by components
  5. `ev_user`: list[str] -- filter logs by users
  6. `ev_op`: list[str] -- filter logs by operations
  7. `ev_session_id`: list[int] -- filter logs by session identificators
  8. `ev_req_id`: list[int] -- filter logs by request identificators
  9. `<attr>`: list[int] -- filter logs by user "attr" specified in "attributes" dict. For example, if user made two POSTs:
  
  POST1
```json
    POST: /audit/events/
    ...
    {
      ...
      "attributes": {
        "policy": "policy1"
      }
    }
```
  POST2
```json
    POST: /audit/events/
    ...
    {
      ...
      "attributes": {
        "policy": "policy2",
        "group": "group1"
      }
    }
```
  then, when client do 
```
    GET: /audit/events/query?policy=policy2
```
  he will get JSON from POST2.
```
    GET: /audit/events/query?policy=policy1,policy2
```
  will return JSONs from both POST1 and POST2.
```
    GET: /audit/events/query?group=group1
```
  will return JSON only from POST2 because POST1 JSON do not containt attr "group" in "attributes" dictionary.

  User defined `<"attr">` must not have prefix `ev_`.

Timestamp formats: ISO 8601(`YYYY-MM-DDThh:mm:ss`) or `YYYY-MM-DDThh:mm:ss:micros` where `micros` have 6 digits