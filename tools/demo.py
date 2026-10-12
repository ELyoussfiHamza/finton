#!/usr/bin/env python3
"""Draws three digits, sends each one to finton, and prints what the model answers.

Start finton with the example model first, then :
    python3 tools/demo.py              # finton on localhost:3001
    python3 tools/demo.py 8080         # another port
Standard library only.
"""
import math
import struct
import sys
import urllib.error
import urllib.request

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 3001
URL = f"http://localhost:{PORT}/infer"


def stroke(points, width=1):
    """A 28 x 28 black image with white lines drawn through the points."""
    image = [[0.0] * 28 for _ in range(28)]
    for (r0, c0), (r1, c1) in zip(points, points[1:]):
        steps = max(abs(r1 - r0), abs(c1 - c0), 1)
        for k in range(steps + 1):
            r = round(r0 + (r1 - r0) * k / steps)
            c = round(c0 + (c1 - c0) * k / steps)
            for dr in range(-width, width + 1):
                for dc in range(-width, width + 1):
                    if 0 <= r + dr < 28 and 0 <= c + dc < 28:
                        image[r + dr][c + dc] = 1.0
    return image


DIGITS = {
    0: stroke([(14 + round(8 * math.sin(t / 20 * 2 * math.pi)), 14 + round(5 * math.cos(t / 20 * 2 * math.pi)))
               for t in range(21)]),
    1: stroke([(5, 14), (22, 14)]),
    7: stroke([(6, 8), (6, 20), (22, 12)]),
}


def infer(image):
    body = struct.pack("<784f", *[pixel for row in image for pixel in row])
    response = urllib.request.urlopen(urllib.request.Request(URL, data=body), timeout=10)
    data = response.read()
    return struct.unpack(f"<{len(data) // 4}f", data)


def main():
    correct = 0
    for digit, image in DIGITS.items():
        for row in image[3:25]:
            print("    " + "".join("##" if pixel else "  " for pixel in row[4:24]))
        try:
            scores = infer(image)
        except urllib.error.HTTPError as error:
            print(f"finton answered {error.code} : {error.read().decode()}")
            return 1
        except OSError as error:
            print(f"Could not reach finton on port {PORT} : {error}")
            return 1
        answer = max(range(len(scores)), key=lambda i: scores[i])
        correct += answer == digit
        print(f"    drew a {digit}, the model says {answer}"
              f"   (scores : {' '.join(f'{s:.1f}' for s in scores)})\n")
    print(f"{correct} of {len(DIGITS)} recognised")
    return 0 if correct == len(DIGITS) else 1


if __name__ == "__main__":
    sys.exit(main())
