"""Guided panel walk: you press, turn and flip each physical control once; the bridge confirms every
one from the Inspector's physical switch bits, encoder counters and events, and records anything
that arrives on a different control. Then it lights known states and asks what colour you see,
optionally with a camera photo as evidence. Development firmware only; it never writes the SD card."""
import time

from forge_audio import PANEL_BUTTONS, panel_gesture, ROOT, show_presets_page
import forge_host as host

NAMES = {index: name for name, index in PANEL_BUTTONS.items()}
WHITE = [f"KEY_{n}" for n in range(1, 16)]
BLACK = [f"KEY_{n}" for n in range(16, 26)]
NOTES = ["C3", "D3", "E3", "F3", "G3", "A3", "B3", "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"]
# Logical knobs 1-4 are SW4, SW1, SW2, SW3; hardware encoder index (Inspector order) per panel name.
ENCODER = {"SW1": 0, "SW2": 1, "SW3": 2, "SW4": 3, "SW5": 4, "SW6": 5}
KNOB = {v: k for k, v in ENCODER.items()}
KNOB_SWITCH = {"SW1": "ENC_1_SW", "SW2": "ENC_2_SW", "SW3": "ENC_3_SW", "SW4": "ENC_4_SW", "SW6": "ENC_6_SW"}
COLOURS = ["Red", "Green", "Blue", "Teal", "Yellow", "Pink", "Purple", "Dim white", "Dim blue", "Off", "Something else"]


class Stop(Exception):
    """The operator ended the walk."""


class Walk:
    def __init__(self, device, show, answers, cancel, log=print, poll=0.12):
        self.device, self.show, self.answers, self.cancel, self.log, self.poll = device, show, answers, cancel, log, poll
        self.results, self.seq = [], 0

    # ---- plumbing ---------------------------------------------------------------------------
    def prompt(self, ident, title, text, choices=(), hint="", photo=False):
        self.seq += 1
        self.show({"seq": self.seq, "id": ident, "title": title, "text": text, "choices": list(choices),
                   "hint": hint, "photo": photo, "done": len(self.results)})

    def answer(self, block):
        """The operator's latest answer, or None. 'stop' ends the walk."""
        try: reply = self.answers.get(timeout=self.poll) if block else self.answers.get_nowait()
        except Exception: return None
        if reply.get("value") == "stop": raise Stop()
        return reply

    def record(self, ident, what, result, detail="", **extra):
        self.results.append({"id": ident, "what": what, "result": result, "detail": detail, **extra})
        self.log(f"{result:7} {what}" + (f" — {detail}" if detail else ""))

    def snap(self): return self.device.snapshot()

    def tap(self, *names):
        for name in names:
            for event in panel_gesture(f"tap {name}"): self.device.panel(event); time.sleep(0.03)

    def wait(self, test):
        """Poll until test(snapshot) returns a verdict (non-None); an answer of 'skip' returns 'skip'."""
        while True:
            if self.cancel.is_set(): raise Stop()
            reply = self.answer(False)
            if reply and reply.get("value") == "skip": return "skip"
            verdict = test(self.snap())
            if verdict is not None: return verdict
            time.sleep(self.poll)

    # ---- physical controls -------------------------------------------------------------------
    def keys(self, ident, title, names, how, labels=None):
        """Press each key in order; every physical key-down is checked against the expected one."""
        self.snap()                                                     # move the event cursor past old events
        pending = []                                                    # physical key-downs not yet matched
        def test(s):
            pending.extend(e["id"] for e in s.get("events", []) if e["kind"] == "key_down" and e["value"] & 1)
            return pending.pop(0) if pending else None                  # value bit 1 = physical, bit 2 = injected
        for i, name in enumerate(names):
            label = labels[i] if labels else name
            self.prompt(f"{ident}.{i + 1}", title, f"Press and release {label}.", hint=f"{how} ({i + 1} of {len(names)})")
            want = PANEL_BUTTONS[name]
            got = self.wait(test)
            if got == "skip": self.record(f"{ident}.{i + 1}", label, "skipped")
            elif got == want: self.record(f"{ident}.{i + 1}", label, "pass", f"switch {want}")
            else: self.record(f"{ident}.{i + 1}", label, "fail", f"expected switch {want} ({name}), got {got} ({NAMES.get(got, '?')})")

    def toggle(self):
        # The firmware's "toggle_up" flag is the menu position, which is physically DOWN (DC's unit, 2026-10-05).
        for want, word, place in ((True, "DOWN", "menu"), (False, "UP", "record"), (True, "DOWN", "menu")):
            self.prompt("toggle", "Toggle switch", f"Flip the toggle (far left of the top row) {word} (the {place} position).",
                        hint="Leave it DOWN at the end: CHOMPI then opens the menu instead of recording.")
            got = self.wait(lambda s: True if s["panel"]["physical"]["toggle_up"] == want else None)
            self.record(f"toggle.{place}", f"Toggle {word.lower()} ({place})", "skipped" if got == "skip" else "pass")

    def turns(self):
        signs = {}
        for name in ("SW4", "SW1", "SW2", "SW3", "SW5", "SW6"):
            for word in ("RIGHT (clockwise)", "LEFT (anticlockwise)"):
                base = self.snap()["panel"]["raw_encoder_turns"]
                self.prompt(f"turn.{name}", f"Turn {name}", f"Turn {name} three clicks {word}.",
                            hint="SW6 is the volume: keep the monitoring level low." if name == "SW6" else "")
                def deltas(s): return [a - b for a, b in zip(s["panel"]["raw_encoder_turns"], base)]
                got = self.wait(lambda s: any(abs(d) >= 2 for d in deltas(s)) or None)
                what = f"{name} turned {word.split()[0].lower()}"
                if got == "skip": self.record(f"turn.{name}", what, "skipped"); continue
                time.sleep(0.4)                                         # let the rest of the turn arrive
                delta = deltas(self.snap())
                index = max(range(len(delta)), key=lambda i: abs(delta[i]))   # the knob that moved most
                moved = ", ".join(f"{KNOB[i]} {d:+d}" for i, d in enumerate(delta) if d)
                if index != ENCODER[name]:
                    self.record(f"turn.{name}", what, "fail", f"{KNOB[index]} moved most ({moved})"); continue
                d = delta[index]
                if word.startswith("RIGHT"):
                    signs[name] = 1 if d > 0 else -1
                    self.record(f"turn.{name}", what, "pass", f"counts {d:+d}" + (f" (also {moved})" if moved.count(",") else ""))
                    continue
                # Left must count the other way from right: this knob's right turn, else the other knobs'.
                right = signs.get(name) or (max(set(signs.values()), key=list(signs.values()).count) if signs else 0)
                if right and (d > 0) == (right > 0):
                    self.record(f"turn.{name}", what, "fail", f"counts {d:+d}: same direction as a right turn")
                else: self.record(f"turn.{name}", what, "pass", f"counts {d:+d}")
        if len(set(signs.values())) > 1:
            self.record("turn.direction", "All knobs count the same way", "fail",
                        ", ".join(f"{k} {'+' if v > 0 else '-'}" for k, v in signs.items()))

    def sw5_press(self):
        """SW5's switch is not in the key matrix: its press is panic (with no loop), seen as voices stopping."""
        self.device.send_patch(host.load_patch(ROOT / "presets/07-warm-pad.json")); time.sleep(0.3)
        for n in (57, 64): self.device.note(n, 60)
        time.sleep(0.3)
        try:
            self.prompt("press.SW5", "Press SW5", "A quiet pad is playing. Press SW5 (the knob between PLAY and SW3) once.",
                        hint="SW5's press is panic: the pad should stop at once.")
            got = self.wait(lambda s: True if s["engine"]["active_voices"] == 0 else None)
            self.record("press.SW5", "SW5 press (panic)", "skipped" if got == "skip" else "pass",
                        "" if got == "skip" else "voices stopped while the notes were still held")
        finally:
            for n in (57, 64): self.device.note(n, 0)

    def jack(self):
        for want, word in ((False, "Unplug the cable from CHOMPI's line input"), (True, "Plug it back in")):
            self.prompt("jack", "Line input jack", word + ".", hint="Skip if no line-in cable is wired.")
            got = self.wait(lambda s: True if s["panel"]["physical"]["line_jack"] == want else None)
            self.record(f"jack.{'out' if not want else 'in'}", "Line jack " + ("removed" if not want else "inserted"),
                        "skipped" if got == "skip" else "pass")

    # ---- lights -------------------------------------------------------------------------------
    def ask(self, ident, title, question, expected, choices=COLOURS, hint=""):
        self.prompt(ident, title, question, choices, hint, photo=True)
        while True:
            reply = self.answer(True)
            if self.cancel.is_set(): raise Stop()
            if not reply: continue
            value = reply.get("value")
            extra = {"commanded": self.leds(), **({"photo": reply["image"]} if reply.get("image") else {})}
            if value == "skip": self.record(ident, title, "skipped", **extra); return
            if value in choices:
                ok = value == expected if isinstance(expected, str) else value in expected
                self.record(ident, title, "pass" if ok else "fail", f"answered {value}; expected {expected}", **extra); return

    def leds(self):
        try: return self.snap()["panel"]["leds"]
        except Exception: return None

    def knob_pages(self, pages):
        """Set knobs 1-4 (SW4, SW1, SW2, SW3) to the given pages by virtual presses."""
        state = self.snap()
        for knob, (name, now, want) in enumerate(zip(("ENC_4_SW", "ENC_1_SW", "ENC_2_SW", "ENC_3_SW"),
                                                     state["panel"]["knob_pages"], pages), 1):
            count = host.knob_pages(state["engine"]["patch"], knob)
            for _ in range((want - now) % count): self.tap(name)

    def lights(self):
        self.device.panel({"kind": 5, "id": 0, "value": 0})
        self.device.send_patch(host.load_patch(ROOT / "presets/07-warm-pad.json"))
        # Knob lights show only in the menu position (TAPE): the bridge holds the toggle there virtually.
        for event in panel_gesture("toggle menu"): self.device.panel(event)
        self.knob_pages([3, 2, 4, 3]); time.sleep(0.2)
        for name, colour in (("SW4", "Red"), ("SW1", "Purple"), ("SW2", "Green"), ("SW3", "Pink")):
            self.ask(f"light.{name}", f"{name} light", f"What colour is the light at {name}?", colour,
                     hint="The bridge put the four knobs on different pages (TAPE's colours, docs/forge/KNOBS.md).")
        self.knob_pages([1, 1, 1, 1])
        for event in panel_gesture("toggle record"): self.device.panel(event)
        time.sleep(0.2)
        self.ask("light.CHOMPI", "CHOMPI light", "With nothing coming in, what colour is the light at the CHOMPI key?",
                 ("Dim white", "Off"), hint="In the record position it is TAPE's input meter: dim white at silence.")
        self.tap("KEY_28"); time.sleep(0.3)                             # first take: PLAY teal, LOOP red
        self.ask("light.PLAY", "PLAY light", "What colour is the PLAY key's light?", ("Teal", "Green"),
                 hint="The bridge started a loop recording. PLAY is KEY_27 and LOOP is KEY_28 on the Panel Map; "
                      "a photo helps.")
        self.ask("light.LOOP", "LOOP light", "What colour is the LOOP key's light?", "Red")
        self.clear_loop()
        for event in panel_gesture("toggle menu") + panel_gesture("hold CHOMPI"): self.device.panel(event)
        time.sleep(0.3)
        if self.snap()["panel"]["menu"]["page"] == "samples": show_presets_page(self.device); time.sleep(0.2)
        self.ask("light.menu", "Menu key lights", "The menu is open on its Presets page. Are the three black keys on the right "
                 "(KEY_23, KEY_24, KEY_25) lit dim red, green and blue, and KEY_22 (PAGE) white?", "Yes", ["Yes", "No"])
        self.device.panel({"kind": 5, "id": 0, "value": 0})

    def clear_loop(self):
        if self.snap()["storage"]["looper"]["state"] == "empty": return
        for gesture in ("hold KEY_27", "hold KEY_28"):
            for event in panel_gesture(gesture): self.device.panel(event)
        time.sleep(2.3)
        for gesture in ("let KEY_27", "let KEY_28", "release all"):
            for event in panel_gesture(gesture): self.device.panel(event)
        time.sleep(0.2)

    # ---- the walk -------------------------------------------------------------------------------
    def run(self, parts=("controls", "lights")):
        started = time.time()
        try:
            self.device.panel({"kind": 5, "id": 0, "value": 0})
            self.device.send_patch(host.load_patch(ROOT / "presets/01-dry.json"))
            if "controls" in parts:
                self.toggle()
                self.keys("white", "White keys", WHITE, "Left to right", [f"white key {n} ({NOTES[n - 1]})" for n in range(1, 16)])
                self.keys("black", "Black keys", BLACK, "Left to right", [f"black key {n} (KEY_{n})" for n in range(16, 26)])
                self.keys("top", "Top-row keys", ["CHOMPI", "KEY_27", "KEY_28"], "With the toggle DOWN (menu position)",
                          ["the CHOMPI key", "PLAY (KEY_27)", "LOOP (KEY_28)"])
                self.keys("knobpress", "Knob presses", ["ENC_4_SW", "ENC_1_SW", "ENC_2_SW", "ENC_3_SW", "ENC_6_SW"],
                          "Push each knob down", ["SW4", "SW1", "SW2", "SW3", "SW6"])
                self.clear_loop()
                self.turns()
                self.sw5_press()
                self.jack()
            if "knobs" in parts and "controls" not in parts:             # re-check the knobs only
                self.clear_loop(); self.turns(); self.sw5_press()
            if "lights" in parts: self.lights()
            stopped = False
        except Stop:
            stopped = True
        finally:
            self.show(None)
            try:
                self.device.panel({"kind": 5, "id": 0, "value": 0})
                self.clear_loop(); self.knob_pages([1, 1, 1, 1])
                self.device.send_patch(host.load_patch(ROOT / "presets/01-dry.json"))
            except Exception as error:
                self.record("cleanup", "Return CHOMPI to the dry patch", "error", str(error))
        counts = {k: sum(r["result"] == k for r in self.results) for k in ("pass", "fail", "skipped", "error")}
        return {"kind": "panel_walk", "counts": counts, "results": self.results, "stopped": stopped,
                "seconds": round(time.time() - started, 1)}
