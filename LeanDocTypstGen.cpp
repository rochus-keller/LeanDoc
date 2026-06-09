/*
* Copyright 2026 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the LeanDoc document language project.
*
* The following is the license that applies to this copy of the
* file. For a license to use the file under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/
#include "LeanDocTypstGen.h"
using namespace LeanDoc;

static bool failAt(TypstGenError* err, const Node* n, const QString& msg)
{
    if( err) {
        err->line = n ? n->pos.row : 0;
        err->message = msg;
    }
    return false;
}

QString TypstGenerator::escString(const QString& s)
{
    QString r;
    r.reserve(s.size() + 8);
    for( int i=0;i<s.size();++i ) {
        const QChar c = s[i];
        if( c == '\\')
            r += "\\\\";
        else if( c == '\"')
            r += "\\\"";
        else if( c == '\n')
            r += "\\n";
        else if( c == '\r')
            { /* drop */ }
        else
            r += c;
    }
    return r;
}

QString TypstGenerator::escText(const QString& s)
{
    QString r;
    r.reserve(s.size() + 16);
    for( int i=0;i<s.size();++i ) {
        const QChar c = s[i];
        if( c == '\\' || c == '*' || c == '_' || c == '`' || c == '#' ||
            c == '[' || c == ']' || c == '<' || c == '>' || c == '@' ) {
            r += '\\';
        }
        r += c;
    }
    return r;
}

QString TypstGenerator::labelSuffix(const BlockMeta* m)
{
    if( !m )
        return "";
    if( m->anchorId.isEmpty() )
        return "";
    return " <" + m->anchorId + ">";
}

QString TypstGenerator::headingMarks(int level)
{
    if( level < 1)
        level = 1;
    if( level > 6)
        level = 6;
    return QString(level, '=');
}

bool TypstGenerator::generate(const Node* doc, QTextStream& out, TypstGenError* err)
{
    if( !doc || doc->kind != Node::K_Document)
        return failAt(err, doc, "Root node is not a Document");

    if( !emitPreamble(doc, out, err))
        return false;

    const QString title = doc->kv.value("title");
    if( !title.isEmpty()) {
        out << "#align(center)[\n";
        out << "  #text(size: 20pt, weight: \"bold\")[" << escText(title) << "]\n";
        out << "]\n\n";
    }

    // emit TOC if :toc: attribute present
    if( doc->kv.contains("attr:toc"))
        out << "#outline(depth: 3)\n#pagebreak()\n\n";

    // shift heading levels: doc title is level 1 (extracted), body starts at level 2
    int shift = doc->kv.contains("title") ? -1 : 0;
    for( int i=0;i<doc->children.size();++i) {
        if( !emitNode(doc->children[i], out, err, shift))
            return false;
        out << "\n";
    }
    return true;
}

bool TypstGenerator::emitPreamble(const Node* doc, QTextStream& out, TypstGenError* err)
{
    if( !dopt.templateFile.isEmpty()) {
        out << "#import \"" << escString(dopt.templateFile) << "\": *\n\n";
        return true;
    }

    bool numbered = doc->kv.contains("attr:numbered") || doc->kv.contains("attr:sectnums");

    if( dopt.templateName == "plain") {
        out << "// LeanDoc -> Typst (plain)\n";
        out << "#set page(margin: 2cm)\n";
        if( numbered)
            out << "#set heading(numbering: \"1.\")\n";
        out << "#set text(font: \"FreeSans\", size: 11pt)\n\n";
        out << "#let admon(kind, body) = block(\n"
               "  inset: (x: 10pt, y: 8pt),\n"
               "  radius: 4pt,\n"
               "  fill: luma(240),\n"
               "  stroke: luma(200),\n"
               "  [*#kind:* ] + body,\n"
               ")\n\n";
        return true;
    }

    if( dopt.templateName == "report") {
        out << "// LeanDoc -> Typst (report)\n";
        out << "#set page(margin: (top: 2cm, bottom: 2.2cm, x: 2.2cm))\n";
        if( numbered)
            out << "#set heading(numbering: \"1.\")\n";
        out << "#set text(font: \"FreeSerif\", size: 11pt, leading: 1.25em)\n\n";
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
    if( !n )
        return true;

    switch( n->kind ) {
    case Node::K_Section:
        return emitSection(n, out, err, headingShift);
    case Node::K_Paragraph:
        return emitParagraph(n, out, err);
    case Node::K_LiteralParagraph:
        return emitLiteral(n, out, err);
    case Node::K_AdmonitionParagraph:
        return emitAdmonition(n, out, err);
    case Node::K_DelimitedBlock:
        return emitDelimited(n, out, err);
    case Node::K_List:
        return emitList(n, out, err);
    case Node::K_Table:
        return emitTable(n, out, err);
    case Node::K_BlockMacro:
        return emitBlockMacro(n, out, err);
    case Node::K_Directive:
        return emitDirective(n, out, err);
    case Node::K_ThematicBreak:
        out << "---\n";
        return true;
    case Node::K_PageBreak:
        out << "#pagebreak()\n";
        return true;
    case Node::K_LineComment:
        out << "// " << escText(n->text) << "\n";
        return true;
    default:
        return failAt(err, n, "Unsupported block node kind in generator");
    }
}

bool TypstGenerator::emitSection(const Node* n, QTextStream& out, TypstGenError* err, int headingShift)
{
    int level = n->level + headingShift;
    if( level < 1 )
        level = 1;

    out << headingMarks(level) << " ";
    if( !n->titleChildren.isEmpty()) {
        if( !emitInlineSeq(n->titleChildren, out, err))
            return false;
    } else {
        out << escText(n->name);
    }
    out << labelSuffix(n->meta) << "\n\n";
    for( int i=0;i<n->children.size();++i ) {
        if( !emitNode(n->children[i], out, err, headingShift))
            return false;
        out << "\n";
    }
    return true;
}

bool TypstGenerator::emitParagraph(const Node* n, QTextStream& out, TypstGenError* err)
{
    if( !emitInlineSeq(n->children, out, err))
        return false;
    out << labelSuffix(n->meta) << "\n";
    return true;
}

bool TypstGenerator::emitLiteral(const Node* n, QTextStream& out, TypstGenError* err)
{
    Q_UNUSED(err);
    out << "#raw(\"" << escString(n->text) << "\", block: true)\n";
    return true;
}

bool TypstGenerator::emitAdmonition(const Node* n, QTextStream& out, TypstGenError* err)
{
    out << "#admon(\"" << escString(n->name) << "\", [";
    if( !emitInlineSeq(n->children, out, err))
        return false;
    out << "])" << labelSuffix(n->meta) << "\n";
    return true;
}

bool TypstGenerator::emitDelimited(const Node* n, QTextStream& out, TypstGenError* err)
{
    if( n->delimKind == Node::DK_Comment)
        return true;

    const QString lbl = labelSuffix(n->meta);
    if( !n->children.isEmpty()) {
        out << "#block([";
        for( int i=0;i<n->children.size();++i) {
            if( !emitNode(n->children[i], out, err, 0))
                return false;
            out << "\n";
        }
        out << "])" << lbl << "\n";
        return true;
    }

    out << "#raw(\"" << escString(n->text) << "\", block: true)" << lbl << "\n";
    return true;
}

bool TypstGenerator::emitList(const Node* n, QTextStream& out, TypstGenError* err)
{
    if( n->listType == Node::LT_Description) {
        out << "#table(columns: 2,\n";
        for( int i=0;i<n->children.size();++i) {
            const Node* it = n->children[i];
            if( !it || it->kind != Node::K_ListItem)
                continue;
            out << "  [" << escText(it->name) << "], ";
            out << "[";
            if( !it->children.isEmpty()) {
                if( !emitNode(it->children[0], out, err, 0))
                    return false;
            }
            out << "],\n";
        }
        out << ")" << labelSuffix(n->meta) << "\n";
        return true;
    }

    bool ordered = (n->listType == Node::LT_Ordered);
    out << (ordered ? "#enum(" : "#list(") << "\n";
    for( int i=0;i<n->children.size();++i) {
        const Node* it = n->children[i];
        if( !it || it->kind != Node::K_ListItem)
            continue;

        out << "  [";
        for( int k=0;k<it->children.size();++k) {
            if( !emitNode(it->children[k], out, err, 0))
                return false;
            if( k+1 < it->children.size())
                out << "\n";
        }
        out << "],\n";
    }
    out << ")" << labelSuffix(n->meta) << "\n";
    return true;
}

static QString typstColSpec(const BlockMeta* m, int fallbackCols)
{
    if( !m || !m->attrs.contains("cols"))
        return QString::number(fallbackCols);

    // parse cols="1,2,3" into (1fr, 2fr, 3fr)
    QString v = m->attrs.value("cols");
    if( v.startsWith('"') && v.endsWith('"'))
        v = v.mid(1, v.size()-2);
    const QStringList parts = v.split(',');

    // check if all parts are numeric (relative widths)
    bool allNumeric = true;
    for( int i = 0; i < parts.size(); ++i) {
        bool ok = false;
        parts[i].trimmed().toInt(&ok);
        if( !ok) { allNumeric = false; break; }
    }

    if( allNumeric && !parts.isEmpty()) {
        QString r = "(";
        for( int i = 0; i < parts.size(); ++i) {
            if( i > 0) r += ", ";
            r += parts[i].trimmed() + "fr";
        }
        r += ")";
        return r;
    }

    return QString::number(fallbackCols);
}

bool TypstGenerator::emitTable(const Node* n, QTextStream& out, TypstGenError* err)
{
    int cols = 0;
    for( int i=0;i<n->children.size();++i) {
        const Node* row = n->children[i];
        if( !row || row->kind != Node::K_TableRow)
            continue;
        cols = row->children.size();
        break;
    }
    if( cols <= 0)
        return true;

    const bool hasHeader = n->kv.contains("header") ||
        (n->meta && n->meta->attrs.contains("options") &&
         n->meta->attrs.value("options").contains("header"));

    out << "#table(columns: " << typstColSpec(n->meta, cols) << ",\n";
    if( hasHeader)
        out << "  table.header(\n";

    for( int i=0;i<n->children.size();++i) {
        const Node* row = n->children[i];
        if( !row || row->kind != Node::K_TableRow)
            continue;

        for( int c=0;c<row->children.size();++c) {
            const Node* cell = row->children[c];
            out << "  [";
            if( cell) {
                if( !emitInlineSeq(cell->children, out, err))
                    return false;
            }
            out << "],\n";
        }
        if( hasHeader && i == 0)
            out << "  ),\n";
    }
    out << ")" << labelSuffix(n->meta) << "\n";
    return true;
}

bool TypstGenerator::emitBlockMacro(const Node* n, QTextStream& out, TypstGenError* err)
{
    if( n->name == "include")
        return failAt(err, n, "include:: requires semantic include expansion before Typst generation");

    if( n->name == "image") {
        QString t = n->target.trimmed();
        int lb = t.indexOf('[');
        QString path = (lb < 0) ? t : t.left(lb).trimmed();
        out << "#image(\"" << escString(path) << "\")" << labelSuffix(n->meta) << "\n";
        return true;
    }

    if( n->name == "video" || n->name == "audio") {
        out << "#link(\"" << escString(n->name + "::" + n->target.trimmed()) << "\")["
            << escText(n->name.toUpper() + ": " + n->target.trimmed()) << "]\n";
        return true;
    }

    return failAt(err, n, "Unsupported block macro in Typst generator: " + n->name);
}

bool TypstGenerator::emitDirective(const Node* n, QTextStream& out, TypstGenError* err)
{
    Q_UNUSED(out);
    return failAt(err, n, "Directives must be resolved before Typst generation (" + n->name + ")");
}

bool TypstGenerator::emitInlineSeq(const QList<Node*>& inl, QTextStream& out, TypstGenError* err)
{
    for( int i=0;i<inl.size();++i) {
        if( !emitInline(inl[i], out, err))
            return false;
    }
    return true;
}

bool TypstGenerator::emitInline(const Node* n, QTextStream& out, TypstGenError* err)
{
    if( !n) return true;

    switch( n->kind ) {
    case Node::K_Text:
        out << escText(n->text);
        return true;

    case Node::K_Bold:
        out << "#strong[";
        if( !emitInlineSeq(n->children, out, err))
            return false;
        out << "]";
        return true;

    case Node::K_Italic:
        out << "#emph[";
        if( !emitInlineSeq(n->children, out, err))
            return false;
        out << "]";
        return true;

    case Node::K_Monospace:
        if( !n->text.isEmpty())
            out << "`" << n->text << "`";
        else {
            out << "`";
            if( !emitInlineSeq(n->children, out, err))
                return false;
            out << "`";
        }
        return true;

    case Node::K_Highlight:
        out << "#highlight([";
        if( !emitInlineSeq(n->children, out, err))
            return false;
        out << "])";
        return true;

    case Node::K_Superscript:
        out << "#super[" << escText(n->text) << "]";
        return true;

    case Node::K_Subscript:
        out << "#sub[" << escText(n->text) << "]";
        return true;

    case Node::K_Link:
        if( n->children.isEmpty()) {
            out << "#link(\"" << escString(n->target) << "\")[" << escText(n->target) << "]";
        } else {
            out << "#link(\"" << escString(n->target) << "\")[";
            if( !emitInlineSeq(n->children, out, err))
                return false;
            out << "]";
        }
        return true;

    case Node::K_Xref:
        if( n->children.isEmpty()) {
            out << "#link(<" << n->target << ">)[" << escText(n->target) << "]";
        } else {
            out << "#link(<" << n->target << ">)[";
            if( !emitInlineSeq(n->children, out, err))
                return false;
            out << "]";
        }
        return true;

    case Node::K_AnchorInline:
        out << "#metadata(none) <" << n->name << ">";
        return true;

    case Node::K_AttrRef:
        out << "{" << escText(n->name) << "}";
        return true;

    case Node::K_LineBreak:
        out << " \\\n";
        return true;

    case Node::K_InlineMacro:
        if( n->name == "footnote") {
            out << "#footnote[";
            if( !emitInlineSeq(n->children, out, err))
                return false;
            out << "]";
            return true;
        }
        if( n->name == "kbd" || n->name == "btn" || n->name == "menu") {
            out << "#smallcaps[";
            if( !emitInlineSeq(n->children, out, err))
                return false;
            out << "]";
            return true;
        }
        if( n->name == "stem") {
            out << "$" << escText(n->target) << "$";
            return true;
        }
        return failAt(err, n, "Unsupported inline macro in Typst generator: " + n->name);

    default:
        return failAt(err, n, "Unsupported inline node kind in generator");
    }
}

