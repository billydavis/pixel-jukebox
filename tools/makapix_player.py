"""Walk the Makapix Club player API over HTTPS from a PC.

Provision a player, wait for it to be linked on makapix.club, store the bearer
token, query 64x64 GIFs and download a few. Standard library only.

    python tools/makapix_player.py                 # provision (first run), query, download
    python tools/makapix_player.py --channel hashtag --hashtag landscape --download 5
    python tools/makapix_player.py --rotate        # mint a new token if the stored one is lost
    python tools/makapix_player.py --reset         # forget this player and start over

Protocol reference: makapix/docs/player/ (quickstart.md, https-connection.md).
"""

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

BASE_URL = "https://makapix.club/api"
DEVICE_MODEL = "pixel-jukebox-64x64"
FIRMWARE_VERSION = "0.1.0"
USER_AGENT = f"pixel-jukebox/{FIRMWARE_VERSION}"

TOOLS_DIR = Path(__file__).resolve().parent
STATE_FILE = TOOLS_DIR / ".player_state.json"
DOWNLOAD_DIR = TOOLS_DIR / "downloads"

POLL_INTERVAL_S = 5
POLL_TIMEOUT_S = 15 * 60  # registration codes expire after 15 minutes


def call(method, path, body=None, token=None):
    """Send a JSON request and return (status, parsed body or None)."""
    headers = {"User-Agent": USER_AGENT, "Accept": "application/json"}
    data = None
    if body is not None:
        data = json.dumps(body).encode()
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = f"Bearer {token}"

    request = urllib.request.Request(BASE_URL + path, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            status, raw = response.status, response.read()
    except urllib.error.HTTPError as error:
        status, raw = error.code, error.read()

    try:
        return status, json.loads(raw) if raw else None
    except json.JSONDecodeError:
        return status, None


def load_state():
    if STATE_FILE.exists():
        return json.loads(STATE_FILE.read_text())
    return {}


def save_state(state):
    STATE_FILE.write_text(json.dumps(state, indent=2))


def provision(state):
    status, body = call(
        "POST",
        "/player/provision",
        {"device_model": DEVICE_MODEL, "firmware_version": FIRMWARE_VERSION},
    )
    if status != 201 or not body:
        sys.exit(f"Provision failed: HTTP {status} {body}")

    state["player_key"] = body["player_key"]
    save_state(state)

    print(f"\nRegistration code: {body['registration_code']}")
    print("Enter it at https://makapix.club under Settings > Players > Add Device.")
    print(f"The code expires at {body['registration_code_expires_at']}.\n")


def wait_for_token(state):
    """Poll the credentials endpoint until the player has been linked."""
    deadline = time.monotonic() + POLL_TIMEOUT_S
    print("Waiting for the device to be linked", end="", flush=True)
    while time.monotonic() < deadline:
        status, body = call("GET", f"/player/{state['player_key']}/credentials")
        if status == 200 and body:
            print(" linked.")
            token = body.get("api_token")
            if not token:
                sys.exit(
                    "Linked, but the token was already issued on an earlier fetch. "
                    "Run again with --rotate to mint a new one."
                )
            state["api_token"] = token
            save_state(state)
            return
        if status == 404:
            time.sleep(POLL_INTERVAL_S)
        elif status == 429:
            time.sleep(60)
        else:
            sys.exit(f"\nCredentials request failed: HTTP {status} {body}")
        print(".", end="", flush=True)
    sys.exit("\nTimed out; the code has expired. Run again with --reset.")


def rotate_token(state):
    if "player_key" not in state:
        sys.exit("No stored player to rotate a token for. Run without --rotate first.")
    status, body = call("POST", f"/player/{state['player_key']}/token/rotate")
    if status != 200 or not body or not body.get("api_token"):
        sys.exit(f"Token rotation failed: HTTP {status} {body}")
    state["api_token"] = body["api_token"]
    save_state(state)
    print("New token stored.")


def query_posts(state, args):
    request = {
        "request_type": "query_posts",
        "channel": args.channel,
        "sort": "random",
        "limit": args.limit,
        # Native GIFs only, so art_url is already the file the firmware will decode.
        "criteria": [
            {"field": "width", "op": "eq", "value": args.size},
            {"field": "height", "op": "eq", "value": args.size},
            {"field": "native_file_format", "op": "eq", "value": "gif"},
        ],
        "include_fields": ["owner_handle", "width", "height", "frame_count", "dwell_time_ms"],
    }
    if args.channel == "hashtag":
        request["hashtag"] = args.hashtag
    elif args.channel == "by_user":
        request["user_handle"] = args.user

    status, body = call("POST", "/player/rpc", request, token=state["api_token"])
    if status == 401:
        sys.exit("Token rejected (HTTP 401). Run again with --rotate.")
    if status != 200 or not body or not body.get("success"):
        sys.exit(f"Query failed: HTTP {status} {body}")
    return body["posts"]


def download(post):
    DOWNLOAD_DIR.mkdir(exist_ok=True)
    target = DOWNLOAD_DIR / f"{post['post_id']}.{post['native_format']}"
    request = urllib.request.Request(post["art_url"], headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=30) as response:
        target.write_bytes(response.read())
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--channel", default="promoted",
                        choices=["all", "promoted", "user", "by_user", "hashtag"])
    parser.add_argument("--hashtag", help="hashtag without the #, for --channel hashtag")
    parser.add_argument("--user", help="user handle, for --channel by_user")
    parser.add_argument("--size", type=int, default=64, help="artwork width and height")
    parser.add_argument("--limit", type=int, default=10, help="posts to query (1-50)")
    parser.add_argument("--download", type=int, default=3, help="artworks to download")
    parser.add_argument("--rotate", action="store_true", help="mint a new bearer token")
    parser.add_argument("--reset", action="store_true", help="forget the stored player")
    args = parser.parse_args()

    if args.channel == "hashtag" and not args.hashtag:
        parser.error("--channel hashtag needs --hashtag")
    if args.channel == "by_user" and not args.user:
        parser.error("--channel by_user needs --user")

    if args.reset:
        STATE_FILE.unlink(missing_ok=True)
        print("Stored player forgotten.")
        return

    state = load_state()
    if args.rotate:
        rotate_token(state)
    if "player_key" not in state:
        provision(state)
    if "api_token" not in state:
        wait_for_token(state)

    posts = query_posts(state, args)
    print(f"{len(posts)} posts from '{args.channel}':")
    for post in posts:
        print(f"  #{post['post_id']}  {post.get('width')}x{post.get('height')}  "
              f"{post.get('frame_count')} frames  by {post.get('owner_handle')}")

    for post in posts[: args.download]:
        print(f"Saved {download(post)}")


if __name__ == "__main__":
    main()
