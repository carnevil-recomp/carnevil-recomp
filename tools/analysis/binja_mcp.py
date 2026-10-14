#!/usr/bin/env python3
"""Minimal MCP-over-HTTP client for the Binary Ninja UI server. Usage: mcp.py <tool> '<json args>'  |  mcp.py list"""
import sys, json, urllib.request, os
URL="http://127.0.0.1:24642/mcp"; SESS=os.path.join(os.path.dirname(__file__),".mcp_session")
def post(payload, sid=None):
    h={"Content-Type":"application/json","Accept":"application/json, text/event-stream"}
    if sid: h["MCP-Session-Id"]=sid
    r=urllib.request.Request(URL,data=json.dumps(payload).encode(),headers=h)
    with urllib.request.urlopen(r,timeout=600) as resp:
        return resp.headers.get("MCP-Session-Id"), resp.read().decode()
def session():
    if os.path.exists(SESS): return open(SESS).read().strip()
    sid,_=post({"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"cc","version":"1"}}})
    post({"jsonrpc":"2.0","method":"notifications/initialized"},sid)
    open(SESS,"w").write(sid); return sid
def parse(body):
    if body.startswith("event:") or body.startswith("data:"):
        for line in body.splitlines():
            if line.startswith("data:"): body=line[5:].strip()
    return json.loads(body)
sid=session()
if sys.argv[1]=="list":
    _,b=post({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}},sid)
    for t in parse(b)["result"]["tools"]: print(t["name"],"-",t.get("description","")[:110].replace("\n"," "))
else:
    args=json.loads(sys.argv[2]) if len(sys.argv)>2 else {}
    _,b=post({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":sys.argv[1],"arguments":args}},sid)
    r=parse(b)
    if "error" in r: print("ERROR",r["error"]); sys.exit(1)
    for c in r["result"].get("content",[]):
        print(c.get("text") if c.get("type")=="text" else c)
