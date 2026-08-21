# ⏱️ Dyadic Chronometer

*A Providence Scientific Instrument — Catalog No. II, "The Base-2 Timepiece"*
*(Workshop designation: **TimeDilator — ESP32 Edition, core build**, per the attending engineer's own notes, June 2026)*

> *"Why tell the hour in mere decimal, when the truer, older mathematics of TWO yet lingers beneath the world?"*
> — attributed to the founding partners of Providence Scientific, extracted from "Hypnogogic Contemplations of Chronomancy".

---

## 🏛️ A Word From the Manufactory

**Providence Scientific** is a purveyor of arcane-scientific instrumentation, headquartered in a leaning brick building off Weybosset Street, Providence, Rhode Island, in an era that is technically the present day but insists on behaving like 1890. We deal in electro-mechanical whatsits, chronographic gizmos, and astronomical doo-dads for the discerning explorer of the parascientific — the sort of person who wakes at 3 AM having solved cosmology in a dream, and requires, by breakfast, an instrument to show for it.

The **Dyadic Chronometer** is our maiden product: a timepiece that has dispensed with the tyranny of Arabic numerals entirely and tells the hour the way the universe actually counts — in binary, one glowing bit at a time. It uses no hands and no mercy for the numerically illiterate. It simply lights up, digit by digit, until you have deciphered the present moment yourself — and then, on the hour, it *chimes*, because Providence Scientific has never met a silence it didn't want to fill with tone.

Providence Scientific accepts no liability for any cosmological discoveries made while squinting at this device before coffee.

---

## 🔦 What It Actually Does

Underneath the brass-and-varnish framing, the Dyadic Chronometer is a **binary LED clock** built on a **NodeMCU ESP32-S**, which reaches out over your household's Wi-Fi to consult the outside world's NTP servers for the correct time, rather than trusting any crystal of its own. It speaks through a single 20-LED string of **WS2811** addressable RGB pixels, and — new to this build — announces itself with a **passive buzzer**, chiming out the hour in ascending tones.

Six rows, six digits, each glowing out its value in binary — MSB on the left, illuminated bulb = 1, dim bulb = 0:

| Row | Digit | Bits | Range |
|---|---|---|---|
| 1 (top) | Hour — tens | 2 | 0–2 |
| 2 | Hour — ones | 4 | 0–9 |
| 3 | Minute — tens | 3 | 0–5 |
| 4 | Minute — ones | 4 | 0–9 |
| 5 | Second — tens | 3 | 0–5 |
| 6 (bottom) | Second — ones | 4 | 0–9 |

Each row is only as wide as its digit actually requires — the tens-of-hours row, for instance, never needs to count past 2, and so gets only two bits, not four. This is not a manufacturing defect. This is *thrift*. Twenty bulbs in total, no more, no fewer, every one of them earning its keep.

So at, say, 14:32:07, the six rows glow out `01`, `0100`, `011`, `0010`, `000`, `0111` — hour tens 1, hour ones 4, minute tens 3, minute ones 2, second tens 0, second ones 7. Reading the time is, in short, an exercise left to the beholder — much like reading the portents, or reading the Necronomicon, or reading an analog clock in a hurry.

### The Chronochromatic Engine

The instrument does not content itself with a single, dreary "on" colour. Illuminated bits are tinted according to the current hour, sweeping through the full ROYGBIV wheel over the course of a day — red at midnight, warming through orange and gold by morning, cooling through green and cyan by noon, and drifting into blue, indigo, and violet as evening falls, before wrapping back to red. Unlit bits glow a faint, dim white, just enough to confirm the bulb is present and merely abstaining, rather than burnt out or possessed.

### The Hourly Carillon

On the striking of every hour, the instrument sounds itself: a run of ascending tones, one note per hour elapsed since midnight (the toll for midnight itself runs the full 24). The notes climb a whole-tone scale by default, though a second tuning — pure just-intonation ratios, for the purist who finds equal temperament a modern indignity — sits dormant in the source, one line-swap away from active duty.

### The Waking Ritual

Power the instrument on, and before it will consent to tell you the time, it performs a small overture: the seconds-rows sweep white from 0 to 59, then the minutes-rows do the same, then the hours-rows sweep through their own colours from 0 to 23 — each sweep chiming along as it goes. Consider it the Chronometer clearing its throat before speaking.

---

## 🗄️ The Cabinet

<p align="center">
  <img src="docs/enclosure.png" alt="Dyadic Chronometer enclosure, showing the drilled bit-pattern for each row" width="380">
</p>

Note the drilling: the top row bears only two bores, the third and fifth rows only three, and the rest a full four — no wasted holes, no wasted bulbs, no wasted brass. A cabinet that knows exactly how much binary it needs to hold, and not one bit more.

---

## ⚙️ Apparatus & Materials

To construct your own Chronometer, the workshop requires:

- **1× NodeMCU ESP32-S** development board — the small silicon oracle at the heart of the thing, now with opinions about Wi-Fi
- **1× WS2811 addressable RGB LED string, 20 LEDs**
- **1× passive buzzer** — for the hourly carillon
- **2× momentary pushbuttons** (normally-open, other leg to ground) — for manually nudging the displayed hour forward or backward
- Hookup wire, a 5V supply appropriate to 20 LEDs at full brightness, and a diffuser/enclosure of your choosing (frosted acrylic is popular among the parascientific set)

### Wiring

| Component | Connects To |
|---|---|
| LED data | GPIO13 |
| Hour forward button | GPIO12 (other leg → GND) |
| Hour backward button | GPIO14 (other leg → GND) |
| Buzzer signal | GPIO27 (other leg → GND) |
| LED VCC | 5V |
| LED GND | GND |

Full LED index mapping (which physical bulb corresponds to which bit, per row) lives in the comments at the top of the sketch — consult it before you start gluing anything into an enclosure.

---

## 🔧 Installation & Flashing

1. Clone this repository into your local workshop:
   ```bash
   git clone https://github.com/coolguyalex/DyadicChronometer.git
   ```
2. Open the sketch in the **Arduino IDE**, with ESP32 board support installed.
3. Install the required library via the Library Manager: `FastLED`. `WiFi.h` and `time.h` ship with the ESP32 core.
4. Wire up the LED string, buzzer, and buttons per the table above.
5. **Configure your network and timezone** near the top of the sketch:
   - Set `WIFI_SSID` and `WIFI_PASSWORD` to your own network's credentials.
   - Set `UTC_OFFSET_SEC` to your **standard-time** offset (e.g. `-5 * 3600` for Eastern), and `DST_OFFSET_SEC` to `3600` if your region observes daylight saving — the sketch applies the DST adjustment automatically.

   > ⚠️ **A word of caution from the workshop's legal department:** the sketch as it ships keeps your Wi-Fi credentials in plain text, right there in the source. That's harmless on a bench, but if this repository is public, so are your credentials, the moment they're committed. Consider keeping your real `WIFI_SSID` / `WIFI_PASSWORD` out of version control — a local `secrets.h` (added to `.gitignore`) or similar — and double-check none of your own have slipped into a commit already.
6. Flash the ESP32, then power on. The onboard string glows dim blue while it hunts for your network, flashes green three times on a successful NTP sync (or a slow red pulse if it can't reach one), then runs its waking ritual and settles into telling the time.
7. Open the Serial Monitor at **115200 baud** if you'd like to watch the hour/minute/second and connection status tick by in plain, unglamorous decimal, for debugging purposes only.

If the display sits on dim red indefinitely, the instrument has connected to nothing — check your Wi-Fi credentials and signal strength before assuming it has achieved sentience and is sulking. This has never happened. Yet.

---

## 👁️ Usage

Glance at the grid. Decode six digits of binary, mind the differing row widths. Announce the time to the room, loudly, in the manner of someone who has clearly overengineered this. Bask in the quiet judgment of anyone who simply wanted to know if they were late. Then wait for the top of the hour, and let the Chronometer announce it for you, in tones.

Should the displayed hour drift from what you'd prefer — daylight saving oddities, travel, or simple whimsy — press the **forward** button to nudge it ahead or the **backward** button to nudge it back, one hour at a time, up to twelve hours in either direction. This offset lives only in the instrument's memory and resets to zero the moment it loses power; the true time, as reckoned by NTP, is never altered, only your view of it. Minutes and seconds remain the network's business and are not user-adjustable, by design.

---

## 🕸️ Contributing

Providence Scientific welcomes fellow tinkerers, hobbyist arcanists, and anyone with strong opinions about LED diffusion material or whole-tone versus just-intonation chiming. Pull requests, issues, and schematic refinements are all gladly received. Please keep commit messages at least *slightly* more dignified than this README — and please, keep your Wi-Fi password out of them entirely.

---

## 📜 License

Released into the world under the **MIT License** — see [`LICENSE`](LICENSE) for the full incantation. Providence Scientific retains no rights over any cosmological discoveries, sleep-deprived epiphanies, or minor existential crises resulting from staring at binary-encoded digits at 2 AM.

---

*Providence Scientific — instruments for the important discovery you thought of while falling asleep.*
