#include "LeanDocTypstGen.h"

namespace LeanDoc2 {

static bool failAt(TypstGenError* err, const Node* n, const QString& msg)
{
    if (err) { err->line = n ? n->pos.line : 0; err->message = msg; }
    return false;
}

QString TypstGenerator::escString(const QString& s)
{
    QString r;
    r.reserve(s.size() + 8);
    for (int i=0;i<s.size();++i) {
        const QChar c = s[i];
        if (c == '\\') r += "\\\\";
        else if (c == '\"') r += "\\\"";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') { /* drop */ }
        else r += c;
    }
    return r;
}

QString TypstGenerator::escText(const QString& s)
{
    // Conservative escaping to avoid accidental markup interpretation.
    // Typst markup uses *, _, `, #, [, ], <label>, etc.
    QString r;
    r.reserve(s.size() + 16);
    for (int i=0;i<s.size();++i) {
        const QChar c = s[i];
        if (c == '\\' || c == '*' || c == '_' || c == '`' || c == '#' ||
            c == '[' || c == ']' || c == '<' || c == '>' ) {
            r += '\\';
        }
        r += c;
    }
    return r;
}

QString TypstGenerator::labelSuffix(const BlockMeta* m)
{
    if (!m) return "";
    if (m->anchorId.isEmpty()) return "";
    // Typst label syntax: <id>
    return " <" + m->anchorId + ">";
}

QString TypstGenerator::headingMarks(int level)
{
    if (level < 1) level = 1;
    if (level > 6) level = 6;
    return QString(level, '=');
}

bool TypstGenerator::generate(const Node* doc, QTextStream& out, TypstGenError* err)
{
    if (!doc || doc->kind != Node::K_Document)
        return failAt(err, doc, "Root node is not a Document");

    if (!emitPreamble(doc, out, err)) return false;

    // If doc has a title in kv, emit it as a top heading.
    const QString title = doc->kv.value("title");
    if (!title.isEmpty()) {
        out << "= " << escText(title) << "\n\n";
    }

    for (int i=0;i<doc->children.size();++i) {
        if (!emitNode(doc->children[i], out, err, /*headingShift*/0)) return false;
        out << "\n";
    }
    return true;
}

bool TypstGenerator::emitPreamble(const Node* doc, QTextStream& out, TypstGenError* err)
{
    Q_UNUSED(doc);

    // Option 1: import a user template file.
    if (!dopt.templateFile.isEmpty()) {
        // The template is expected to define `project` (or do its own #show). Keep it simple:
        out << "#import \"" << escString(dopt.templateFile) << "\": *\n\n";
        return true;
    }

    // Option 2: built-in templates (simple Typst prelude).
    if (dopt.templateName == "plain") {
        out << "// LeanDoc -> Typst (plain)\n";
        out << "#set page(margin: 2cm)\n";
        out << "#set text(font: \"Linux Libertine\", size: 11pt)\n\n";
        // Minimal admonition helper.
        out << "#let admon(kind, body) = block(\n"
               "  inset: (x: 10pt, y: 8pt),\n"
               "  radius: 4pt,\n"
               "  fill: luma(240),\n"
               "  stroke: luma(200),\n"
               "  [*#kind:* ] + body,\n"
               ")\n\n";
        return true;
    }

    if (dopt.templateName == "report") {
        out << "// LeanDoc -> Typst (report)\n";
        out << "#set page(margin: (top: 2cm, bottom: 2.2cm, x: 2.2cm))\n";
        out << "#set text(font: \"Libertinus Serif\", size: 11pt, leading: 1.25em)\n";
        out << "#set heading(numbering: \"1.\")\n\n";
        out << "#let admon(kind, body) = block(\n"
               "  inset: (x: 12pt, y: 10pt),\n"
               "  radius: 6pt,\n"
               "  fill: rgb(\"f6f7fb\"),\n"
               "  stroke: rgb(\"cfd6e6\"),\n"
               "  [#text(weight: \"bold\")[#kind] ] + body,\n"
               ")\n\n";
        return true;
    }

    return failAt(err, 0, "Unknown templateName: " + dopt.templateName);
}

bool TypstGenerator::emitNode(const Node* n, QTextStream& out, TypstGenError* err, int headingShift)
{
    if (!n) return true;

    switch (n->kind) {
    case Node::K_Section: return emitSection(n, out, err, headingShift);
    case Node::K_Paragraph: return emitParagraph(n, out, err);
    case Node::K_LiteralParagraph: return emitLiteral(n, out, err);
    case Node::K_AdmonitionParagraph: return emitAdmonition(n, out, err);
    case Node::K_DelimitedBlock: return emitDelimited(n, out, err);
    case Node::K_List: return emitList(n, out, err);
    case Node::K_Table: return emitTable(n, out, err);
    case Node::K_BlockMacro: return emitBlockMacro(n, out, err);
    case Node::K_Directive: return emitDirective(n, out, err);
    case Node::K_ThematicBreak: out << "---\n"; return true;
    case Node::K_PageBreak:
        out << "#pagebreak()\n";
        return true;
    case Node::K_LineComment:
        // Drop comments in output (or keep as typst comment).
        out << "// " << escText(n->text) << "\n";
        return true;
    default:
        return failAt(err, n, "Unsupported block node kind in generator");
    }
}

bool TypstGenerator::emitSection(const Node* n, QTextStream& out, TypstGenError* err, int headingShift)
{
    Q_UNUSED(err);
    int level = n->kv.value("level").toInt();
    level += headingShift;
    if (level < 1) level = 1;

    out << headingMarks(level) << " " << escText(n->name) << labelSuffix(n->meta) << "\n\n";
    for (int i=0;i<n->children.size();++i) {
        if (!emitNode(n->children[i], out, err, headingShift)) return false;
        out << "\n";
    }
    return true;
}

bool TypstGenerator::emitParagraph(const Node* n, QTextStream& out, TypstGenError* err)
{
    if (!emitInlineSeq(n->children, out, err)) return false;
    out << "\n";
    return true;
}

bool TypstGenerator::emitLiteral(const Node* n, QTextStream& out, TypstGenError* err)
{
    Q_UNUSED(err);
    // Use raw block for verbatim.
    out << "#raw(\"" << escString(n->text) << "\", block: true)\n";
    return true;
}

bool TypstGenerator::emitAdmonition(const Node* n, QTextStream& out, TypstGenError* err)
{
    out << "#admon(\"" << escString(n->name) << "\", [";
    if (!emitInlineSeq(n->children, out, err)) return false;
    out << "])\n";
    return true;
}

bool TypstGenerator::emitDelimited(const Node* n, QTextStream& out, TypstGenError* err)
{
    // We stored delimiter kind as integer token kind in kv["delim"] in the earlier parser.
    // Here: decide by presence of children vs raw text.
    if (!n->children.isEmpty()) {
        // Container block: render as simple block with its content.
        out << "#block([";
        for (int i=0;i<n->children.size();++i) {
            if (!emitNode(n->children[i], out, err, 0)) return false;
            out << "\n";
        }
        out << "])\n";
        return true;
    }

    // Raw content blocks (listing/literal/passthrough/comment/stem)
    const QString isStem = n->kv.value("stem");
    if (isStem == "1") {
        // Math is complicated; emit as raw Typst for now (semantic checker may convert math).
        if (!dopt.allowRawPassthrough)
            return failAt(err, n, "Stem block requires raw passthrough or math conversion phase");
        out << n->text << "\n";
        return true;
    }

    // For comment blocks, drop.
    // For passthrough blocks, either output as-is or error depending on option.
    if (!dopt.allowRawPassthrough) {
        // Listing/literal can still be #raw, but passthrough should not.
        out << "#raw(\"" << escString(n->text) << "\", block: true)\n";
        return true;
    }

    // If it’s passthrough, output raw; otherwise show as raw code.
    // (If you want strict differentiation, add a dedicated field for delimited-kind.)
    out << "#raw(\"" << escString(n->text) << "\", block: true)\n";
    return true;
}

bool TypstGenerator::emitList(const Node* n, QTextStream& out, TypstGenError* err)
{
    const QString type = n->kv.value("type"); // "unordered"/"ordered"/"description"
    if (type == "description") {
        // Render as a simple description list using a table (term | def) as a robust fallback.
        out << "#table(columns: 2,\n";
        for (int i=0;i<n->children.size();++i) {
            const Node* it = n->children[i];
            if (!it || it->kind != Node::K_ListItem) continue;
            // term
            out << "  [" << escText(it->name) << "], ";
            // definition: render first child (if present) or empty
            out << "[";
            if (!it->children.isEmpty()) {
                // often a paragraph node
                if (!emitNode(it->children[0], out, err, 0)) return false;
            }
            out << "],\n";
        }
        out << ")\n";
        return true;
    }

    // Typst list markup: "- item" works, but to be explicit use #list.
    bool ordered = (type == "ordered");
    out << (ordered ? "#enum(" : "#list(") << "\n";
    for (int i=0;i<n->children.size();++i) {
        const Node* it = n->children[i];
        if (!it || it->kind != Node::K_ListItem) continue;

        out << "  [";
        // render all children of list item (paragraph + continuations)
        for (int k=0;k<it->children.size();++k) {
            if (!emitNode(it->children[k], out, err, 0)) return false;
            if (k+1 < it->children.size()) out << "\n";
        }
        out << "],\n";
    }
    out << ")\n";
    return true;
}

bool TypstGenerator::emitTable(const Node* n, QTextStream& out, TypstGenError* err)
{
    // Expect children to be rows (K_TableRow) and their children to be cells (K_TableCell)
    int cols = 0;
    for (int i=0;i<n->children.size();++i) {
        const Node* row = n->children[i];
        if (!row || row->kind != Node::K_TableRow) continue;
        cols = row->children.size();
        break;
    }
    if (cols <= 0) return true;

    out << "#table(columns: " << cols << ",\n";
    for (int i=0;i<n->children.size();++i) {
        const Node* row = n->children[i];
        if (!row || row->kind != Node::K_TableRow) continue;
        if (row->children.size() != cols)
            return failAt(err, row, "Table row has inconsistent number of cells");

        for (int c=0;c<row->children.size();++c) {
            const Node* cell = row->children[c];
            out << "  [";
            if (cell) {
                if (!emitInlineSeq(cell->children, out, err)) return false;
            }
            out << "],\n";
        }
    }
    out << ")\n";
    return true;
}

bool TypstGenerator::emitBlockMacro(const Node* n, QTextStream& out, TypstGenError* err)
{
    // Parser currently stored only name=head and target=rest (unparsed).
    // For strictness, require semantic stage (or implement full macro parsing here).
    if (n->name == "include")
        return failAt(err, n, "include:: requires semantic include expansion before Typst generation");

    if (n->name == "image") {
        // Expect "path[...]" in target; best-effort split at '['
        QString t = n->target.trimmed();
        int lb = t.indexOf('[');
        QString path = (lb < 0) ? t : t.left(lb).trimmed();
        out << "#image(\"" << escString(path) << "\")\n";
        return true;
    }

    if (n->name == "video" || n->name == "audio") {
        // Typst doesn't have a standard embedded media player in PDF; emit as link placeholder.
        out << "#link(\"" << escString(n->name + "::" + n->target.trimmed()) << "\")["
            << escText(n->name.toUpper() + ": " + n->target.trimmed()) << "]\n";
        return true;
    }

    // Custom macro: keep as comment so semantic phase can handle it.
    return failAt(err, n, "Unsupported block macro in Typst generator: " + n->name);
}

bool TypstGenerator::emitDirective(const Node* n, QTextStream& out, TypstGenError* err)
{
    Q_UNUSED(out);
    // Directives must be resolved by a preprocessor/semantic step.
    return failAt(err, n, "Directives must be resolved before Typst generation (" + n->name + ")");
}

bool TypstGenerator::emitInlineSeq(const QList<Node*>& inl, QTextStream& out, TypstGenError* err)
{
    for (int i=0;i<inl.size();++i) {
        if (!emitInline(inl[i], out, err)) return false;
    }
    return true;
}

bool TypstGenerator::emitInline(const Node* n, QTextStream& out, TypstGenError* err)
{
    if (!n) return true;

    switch (n->kind) {
    case Node::K_Text:
        out << escText(n->text);
        return true;

    case Node::K_Emph:
        if (n->name == "bold") { out << "*"; if (!emitInlineSeq(n->children, out, err)) return false; out << "*"; return true; }
        if (n->name == "italic") { out << "_"; if (!emitInlineSeq(n->children, out, err)) return false; out << "_"; return true; }
        if (n->name == "mono") {
            if (!n->text.isEmpty()) out << "`" << escText(n->text) << "`";
            else { out << "`"; if (!emitInlineSeq(n->children, out, err)) return false; out << "`"; }
            return true;
        }
        if (n->name == "highlight") { out << "#highlight(["; if (!emitInlineSeq(n->children, out, err)) return false; out << "])"; return true; }
        return failAt(err, n, "Unknown inline emphasis kind: " + n->name);

    case Node::K_Superscript:
        // Typst supports super/sub via functions; keep simple:
        out << "#super[" << escText(n->text) << "]";
        return true;

    case Node::K_Subscript:
        out << "#sub[" << escText(n->text) << "]";
        return true;

    case Node::K_Link:
        // If children empty: autolink.
        if (n->children.isEmpty()) {
            out << "#link(\"" << escString(n->target) << "\")[" << escText(n->target) << "]";
        } else {
            out << "#link(\"" << escString(n->target) << "\")[";
            if (!emitInlineSeq(n->children, out, err)) return false;
            out << "]";
        }
        return true;

    case Node::K_Xref:
        if (n->children.isEmpty()) {
            out << "@" << escText(n->target);
        } else {
            out << "#link(<" << escText(n->target) << ">)[";
            if (!emitInlineSeq(n->children, out, err)) return false;
            out << "]";
        }
        return true;

    case Node::K_AnchorInline:
        out << "<" << escText(n->name) << ">";
        return true;

    case Node::K_AttrRef:
        // Semantic checker should substitute; keep as visible placeholder in Typst:
        out << "{";
        out << escText(n->name);
        out << "}";
        return true;

    case Node::K_InlineMacro:
        // Render selected macros; others require semantic phase or custom functions.
        if (n->name == "footnote") {
            out << "#footnote[";
            if (!emitInlineSeq(n->children, out, err)) return false;
            out << "]";
            return true;
        }
        if (n->name == "kbd" || n->name == "btn" || n->name == "menu") {
            out << "#smallcaps[";
            if (!emitInlineSeq(n->children, out, err)) return false;
            out << "]";
            return true;
        }
        if (n->name == "stem") {
            if (!dopt.allowRawPassthrough)
                return failAt(err, n, "stem: inline macro requires raw passthrough or math conversion phase");
            out << "$" << escText(n->target) << "$";
            return true;
        }
        return failAt(err, n, "Unsupported inline macro in Typst generator: " + n->name);

    case Node::K_PassthroughInline:
        if (!dopt.allowRawPassthrough)
            return failAt(err, n, "Inline passthrough disabled");
        // Output its (already parsed) children as-is.
        return emitInlineSeq(n->children, out, err);

    default:
        return failAt(err, n, "Unsupported inline node kind in generator");
    }
}

} // namespace LeanDoc2

