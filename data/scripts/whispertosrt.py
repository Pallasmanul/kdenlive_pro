#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import datetime
import srt
import sys

import whispertotext

# Call this script with the following arguments
# 1. source av file
# 2. model name (tiny, base, small, medium, large)
# 3. output .srt file
# 4. Device (cpu, cuda)
# 5. translate or transcribe
# 6. Language
# 7. in point (optional)
# 8. out point
# 9. tmp file name to extract a clip's part

def main():
    source = sys.argv[1]
    model = sys.argv[2]
    outfile = sys.argv[3]
    device = sys.argv[4]
    task = sys.argv[5]
    language = sys.argv[6]
    if len(sys.argv) > 8:
        whispertotext.extract_zone(source, sys.argv[7], sys.argv[8], sys.argv[9])
        source = sys.argv[9]

    result = whispertotext.run_whisper(source, model, device, task, language)

    subs = []
    for i in range(len(result["segments"])):
        start_time = result["segments"][i]["start"]
        end_time = result["segments"][i]["end"]
        duration = end_time - start_time
        timestamp = f"{start_time:.3f} - {end_time:.3f}"
        text = result["segments"][i]["text"]

        sub = srt.Subtitle(index=len(subs), content=text, start=datetime.timedelta(seconds=start_time), end=datetime.timedelta(seconds=end_time))
        subs.append(sub)

    subtitle = srt.compose(subs)

    with open(outfile, 'w', encoding='utf8') as f:
        f.writelines(subtitle)
    return 0


if __name__ == "__main__":
    sys.exit(main())
