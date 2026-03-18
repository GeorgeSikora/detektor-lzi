from pathlib import Path

from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import Paragraph, SimpleDocTemplate, Spacer


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "docs" / "shrnuti-projektu.md"
OUTPUT = ROOT / "docs" / "detektor-lzi-shrnuti.pdf"


def register_font() -> str:
    candidates = [
        Path("C:/Windows/Fonts/arial.ttf"),
        Path("C:/Windows/Fonts/calibri.ttf"),
        Path("C:/Windows/Fonts/segoeui.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            pdfmetrics.registerFont(TTFont("ProjectFont", str(candidate)))
            return "ProjectFont"
    return "Helvetica"


def build_story(font_name: str) -> list:
    styles = getSampleStyleSheet()
    title_style = ParagraphStyle(
        "ProjectTitle",
        parent=styles["Title"],
        fontName=font_name,
        fontSize=18,
        leading=22,
        spaceAfter=8,
    )
    heading_style = ParagraphStyle(
        "ProjectHeading",
        parent=styles["Heading2"],
        fontName=font_name,
        fontSize=13,
        leading=16,
        spaceBefore=8,
        spaceAfter=4,
    )
    body_style = ParagraphStyle(
        "ProjectBody",
        parent=styles["BodyText"],
        fontName=font_name,
        fontSize=10.5,
        leading=14,
        spaceAfter=3,
    )

    story = []
    for raw_line in SOURCE.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line:
            story.append(Spacer(1, 3))
            continue

        escaped = (
            line.replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
        )

        if line.startswith("# "):
            story.append(Paragraph(escaped[2:], title_style))
        elif line.startswith("## "):
            story.append(Paragraph(escaped[3:], heading_style))
        elif line.startswith("- "):
            story.append(Paragraph(f"• {escaped[2:]}", body_style))
        else:
            story.append(Paragraph(escaped, body_style))
    return story


def main() -> None:
    font_name = register_font()
    doc = SimpleDocTemplate(
        str(OUTPUT),
        pagesize=A4,
        leftMargin=18 * mm,
        rightMargin=18 * mm,
        topMargin=16 * mm,
        bottomMargin=16 * mm,
        title="Detektor LZI - shrnuti projektu",
        author="GitHub Copilot",
    )
    doc.build(build_story(font_name))
    print(f"PDF created: {OUTPUT}")


if __name__ == "__main__":
    main()