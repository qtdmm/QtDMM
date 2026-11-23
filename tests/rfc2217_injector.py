#!/usr/bin/env python3
# Copyright (c) 2025 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later

import sys
import json
import socket
import threading
import time
import curses

# this computer only; a QtDMM on another one: give the address to listen on
# as the second argument (e.g. 0.0.0.0, all interfaces)
HOST = '127.0.0.1'
PORT = 4000
SEND_INTERVAL = 0.5  # Sekunden

# Telnet- und RFC2217-Kontrollzeichen
IAC  = 255
WILL = 251
WONT = 252
DO   = 253
DONT = 254
SB   = 250
SE   = 240

def hex_string_to_bytes(hex_string):
    return bytes(int(b, 16) for b in hex_string.strip().split())

def loadData(json_file):
    testdata = []
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except Exception as e:
        print(f"Error reading JSON file: {e}")
        sys.exit(1)

    for test in data.get("tests", []):
        hex_str = test.get("hex", "")
        expected = test.get("expected", {})
        item = {
            'hex': hex_str,
            'expected': expected,
            'bytes': hex_string_to_bytes(hex_str)
        }
        testdata.append(item)
    return testdata

class RFC2217Server(threading.Thread):
    def __init__(self, testdata):
        super().__init__(daemon=True)
        self.testdata = testdata
        self.index = 0
        self.running = True
        self.client = None
        self.lock = threading.Lock()

    def set_index(self, idx):
        with self.lock:
            self.index = idx

    def stop(self):
        self.running = False
        if self.client:
            try:
                self.client.shutdown(socket.SHUT_RDWR)
                self.client.close()
            except:
                pass

    def handle_telnet_negotiation(self, conn, byte):
        # Lies weitere bytes zur vollständigen Telnet-Sequenz
        option = conn.recv(1)[0]
        if byte in (DO, DONT):
            # Antwort mit WONT auf alles
            conn.send(bytes([IAC, WONT, option]))
        elif byte in (WILL, WONT):
            # Antwort mit DONT auf alles
            conn.send(bytes([IAC, DONT, option]))

    def run(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.bind((HOST, PORT))
            server.listen(1)
            print(f"[INFO] Server läuft auf Port {PORT}, warte auf Verbindung...")

            self.client, addr = server.accept()
            print(f"[INFO] Verbunden mit {addr}")

            self.client.settimeout(0.1)

            while self.running:
                try:
                    # Verarbeitung von RFC2217/Telnet-Kontrollsequenzen
                    try:
                        data = self.client.recv(1024)
                        for b in data:
                            if b == IAC:
                                cmd = self.client.recv(1)[0]
                                self.handle_telnet_negotiation(self.client, cmd)
                    except socket.timeout:
                        pass

                    # Sende aktuellen Datensatz
                    with self.lock:
                        current_data = self.testdata[self.index]['bytes']
                    self.client.sendall(current_data)
                    time.sleep(SEND_INTERVAL)

                except Exception as e:
                    print(f"[WARN] Verbindung beendet: {e}")
                    break

            print("[INFO] Serverthread beendet")

def run_ui(data, server):
    def render(stdscr):
        curses.curs_set(0)
        index = 0
        total = len(data)

        while True:
            stdscr.clear()
            stdscr.addstr(0, 0, f"Index: {index} / {total - 1}")
            stdscr.addstr(2, 0, f"Hex:      {data[index]['hex']}")
            stdscr.addstr(3, 0, f"Expected: {data[index]['expected']}")
            stdscr.addstr(5, 0, "↑ / ↓: Auswahl  |  q: Quit")
            stdscr.refresh()

            key = stdscr.getch()
            if key == ord('q'):
                server.stop()
                break
            elif key == curses.KEY_UP:
                index = (index - 1) % total
                server.set_index(index)
            elif key == curses.KEY_DOWN:
                index = (index + 1) % total
                server.set_index(index)

    curses.wrapper(render)

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <json_file> [listen_address]")
        sys.exit(1)
    if len(sys.argv) > 2:
        global HOST
        HOST = sys.argv[2]

    data = loadData(sys.argv[1])
    if not data:
        print("Keine Testdaten gefunden.")
        sys.exit(1)

    server = RFC2217Server(data)
    server.start()
    run_ui(data, server)

if __name__ == "__main__":
    main()
