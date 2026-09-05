import argparse
import asyncio
from pathlib import Path

import edge_tts


async def run(args):
    text = Path(args.input).read_text(encoding="utf-8-sig").strip()
    if not text:
        raise ValueError("input text is empty")

    # SentenceBoundary produces natural, readable subtitle cues. It is Edge
    # TTS's current default, but explicitly requesting it keeps output stable
    # across edge-tts releases. SubMaker also accepts WordBoundary events.
    communicate = edge_tts.Communicate(
        text,
        args.voice,
        rate=args.rate,
        boundary="SentenceBoundary",
    )
    submaker = edge_tts.SubMaker()
    with Path(args.output).open("wb") as audio:
        async for chunk in communicate.stream():
            if chunk["type"] == "audio":
                audio.write(chunk["data"])
            elif chunk["type"] in ("WordBoundary", "SentenceBoundary"):
                submaker.feed(chunk)
    Path(args.srt).write_text(submaker.get_srt(), encoding="utf-8")


parser = argparse.ArgumentParser(description="Bundled Edge TTS renderer")
parser.add_argument("--input", required=True)
parser.add_argument("--output", required=True)
parser.add_argument("--srt", required=True)
parser.add_argument("--voice", required=True)
parser.add_argument("--rate", required=True)
asyncio.run(run(parser.parse_args()))
