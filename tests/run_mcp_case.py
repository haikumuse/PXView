#!/usr/bin/env python3
"""
run_mcp_case.py - Drive a single MCP JSON-RPC 2.0 test case.

Usage:
    python run_mcp_case.py <path/to/case.json>

Behavior:
    * Reads the JSON request body from the given file.
    * HTTP POSTs it to http://127.0.0.1:10110/mcp with
      Content-Type: application/json.
    * Validates the response:
        - HTTP status 200
        - Body parses as JSON
        - Looks like JSON-RPC 2.0: has "jsonrpc" == "2.0" OR contains
          "result" or "error" OR has an "id" matching the request id.
    * For expected-failure cases (filename contains "_err" or "error"),
      an "error" response is acceptable.
    * Exit code 0 on success, 1 on failure (with diagnostic output).

Only the Python standard library is used (urllib.request).
"""

import json
import os
import sys
import urllib.error
import urllib.request

MCP_URL = "http://127.0.0.1:10110/mcp"
TIMEOUT_SECONDS = 30


def is_error_case(json_path: str) -> bool:
    """A case is treated as 'expected-error' if its basename contains
    '_err' or 'error' (case-insensitive)."""
    base = os.path.basename(json_path).lower()
    return ("_err" in base) or ("error" in base)


def load_request(json_path: str):
    with open(json_path, "r", encoding="utf-8") as f:
        raw = f.read()
    try:
        body = json.loads(raw)
    except json.JSONDecodeError as exc:
        print("FAIL: request file is not valid JSON: {}".format(exc),
              file=sys.stderr)
        print("---- request body ----", file=sys.stderr)
        print(raw, file=sys.stderr)
        return None, raw
    return body, raw


def post_request(raw_body: str):
    """POST raw_body to MCP_URL; return (status, response_text)."""
    data = raw_body.encode("utf-8")
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json",
    }
    req = urllib.request.Request(MCP_URL, data=data, headers=headers,
                                 method="POST")
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT_SECONDS) as resp:
            status = resp.getcode()
            text = resp.read().decode("utf-8", errors="replace")
            return status, text
    except urllib.error.HTTPError as exc:
        # Server responded with non-2xx: still capture body for diagnostics.
        try:
            text = exc.read().decode("utf-8", errors="replace")
        except Exception:
            text = ""
        return exc.code, text
    except urllib.error.URLError as exc:
        print("FAIL: cannot connect to MCP service at {} ({}). "
              "Is PXView --headless running?".format(MCP_URL, exc.reason),
              file=sys.stderr)
        return None, None
    except ConnectionError as exc:
        print("FAIL: connection error to MCP service at {} ({}). "
              "Is PXView --headless running?".format(MCP_URL, exc),
              file=sys.stderr)
        return None, None
    except Exception as exc:
        print("FAIL: unexpected transport error: {}".format(exc),
              file=sys.stderr)
        return None, None


def validate_response(resp_text: str, request_body, expected_error: bool):
    """Return (ok: bool, reason: str)."""
    try:
        resp = json.loads(resp_text)
    except json.JSONDecodeError as exc:
        return False, "response is not valid JSON: {}".format(exc)

    if not isinstance(resp, dict):
        return False, "response JSON is not an object: {!r}".format(resp)

    has_jsonrpc = resp.get("jsonrpc") == "2.0"
    has_result = "result" in resp
    has_error = "error" in resp
    req_id = request_body.get("id") if isinstance(request_body, dict) else None
    id_matches = (req_id is not None) and (resp.get("id") == req_id)

    # Heuristic: at least one of the JSON-RPC 2.0 markers must be present.
    is_jsonrpc = has_jsonrpc or has_result or has_error or id_matches
    if not is_jsonrpc:
        return False, ("response does not look like JSON-RPC 2.0 "
                       "(no jsonrpc/result/error/id): {!r}").format(resp)

    if has_error:
        if expected_error:
            return True, ("expected-error case returned an error response "
                          "(allowed): {!r}").format(resp.get("error"))
        return False, ("unexpected error response: {!r}").format(
            resp.get("error"))

    # Success response with result (or just jsonrpc/id echo).
    return True, "valid JSON-RPC 2.0 response"


def main(argv):
    if len(argv) != 2:
        print("Usage: {} <path/to/case.json>".format(argv[0]),
              file=sys.stderr)
        return 2

    json_path = argv[1]
    if not os.path.isfile(json_path):
        print("FAIL: case file not found: {}".format(json_path),
              file=sys.stderr)
        return 2

    request_body, raw = load_request(json_path)
    if request_body is None:
        return 1

    expected_error = is_error_case(json_path)

    status, resp_text = post_request(raw)
    if status is None:
        # Connection failed; diagnostics already printed.
        return 1

    if status != 200:
        print("FAIL: HTTP status {} (expected 200)".format(status),
              file=sys.stderr)
        print("---- request body ----", file=sys.stderr)
        print(raw, file=sys.stderr)
        if resp_text:
            print("---- response body ----", file=sys.stderr)
            print(resp_text, file=sys.stderr)
        return 1

    if resp_text is None or resp_text == "":
        print("FAIL: empty response body", file=sys.stderr)
        return 1

    ok, reason = validate_response(resp_text, request_body, expected_error)
    if not ok:
        print("FAIL: {}".format(reason), file=sys.stderr)
        print("---- request body ----", file=sys.stderr)
        print(raw, file=sys.stderr)
        print("---- response body ----", file=sys.stderr)
        print(resp_text, file=sys.stderr)
        return 1

    print("PASS: {} [{}] {}".format(
        os.path.basename(json_path),
        "expected-error" if expected_error else "ok",
        reason))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
