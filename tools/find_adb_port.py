"""find_adb_port.py ip [ip ...]: TCP-scan 30000-61000 (where Android's wireless debugging listens) on the owner's own
devices and print each open port as ip:port, for scripts\\find-wireless-adb.ps1 to try with `adb connect`. Used when
mDNS discovery does not work from this PC (it got no answers here on 2026-11-20)."""
import asyncio
import sys

sem = asyncio.Semaphore(400)


async def probe(ip, port, found):
    async with sem:
        try:
            _, w = await asyncio.wait_for(asyncio.open_connection(ip, port), timeout=0.6)
            found.append(f"{ip}:{port}")
            w.close()
        except Exception:
            pass


async def main(ips):
    found = []
    for ip in ips:
        await asyncio.gather(*(probe(ip, p, found) for p in range(30000, 61001)))
    return found


if __name__ == "__main__":
    for hit in asyncio.run(main(sys.argv[1:])):
        print(hit)
