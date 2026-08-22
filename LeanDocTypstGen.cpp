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

static QString unquote(QString s)
{
    if( s.startsWith('"') && s.endsWith('"') && s.size() >= 2)
        s = s.mid(1, s.size()-2);
    return s;
}

static bool hasStyle(const Node* n, const QString& style)
{
    if( !n->meta)
        return false;
    if( n->meta->roles.contains(style))
        return true;
    QMap<QString,QString>::ConstIterator it = n->meta->attrs.constBegin();
    for( ; it != n->meta->attrs.constEnd(); ++it) {
        if( it.key().startsWith("positional") && unquote(it.value()) == style)
            return true;
    }
    return false;
}

static QString romanNumeral(int n)
{
    static const int vals[] =    { 1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1 };
    static const char* syms[] =  { "M", "CM", "D", "CD", "C", "XC", "L",
                                   "XL", "X", "IX", "V", "IV", "I" };
    QString r;
    for( int i = 0; i < 13 && n > 0; ++i) {
        while( n >= vals[i]) {
            r += syms[i];
            n -= vals[i];
        }
    }
    return r;
}

void TypstGenerator::collectRefTargets(const Node* n)
{
    if( !n)
        return;
    if( n->kind == Node::K_Section &&
            (!n->meta || n->meta->anchorId.isEmpty())) {
        // anchor automatically derived from the section title
        // only usable when unique in the document
        const QString id = derivedSectionId(n->name);
        if( !id.isEmpty()) {
            if( dautoLabels.contains(id)) {
                // several titles derive the same ID: none is usable
                dambiguousAutoLabels.insert(id);
                dautoLabels.remove(id);
                drefTargets.remove(id);
            } else if( dambiguousAutoLabels.contains(id) || drefTargets.contains(id)) {
                // already ambiguous, or an explicit anchor owns this ID
                dambiguousAutoLabels.insert(id);
            } else {
                dautoLabels.insert(id);
                RefTarget t;
                t.text = n->name;
                t.numbered = !hasStyle(n, "part") && !hasStyle(n, "discrete");
                drefTargets.insert(id, t);
            }
        }
    }
    if( n->meta && !n->meta->anchorId.isEmpty()) {
        RefTarget t;
        if( !n->meta->anchorText.isEmpty())
            t.text = n->meta->anchorText;
        else if( n->meta->attrs.contains("reftext"))
            t.text = unquote(n->meta->attrs.value("reftext"));
        else if( n->kind == Node::K_Section)
            t.text = n->name;
        if( n->kind == Node::K_Section &&
            !hasStyle(n, "part") && !hasStyle(n, "discrete"))
            t.numbered = true;
        // an explicit anchor also wins over a colliding derived one
        if( dautoLabels.contains(n->meta->anchorId)) {
            dautoLabels.remove(n->meta->anchorId);
            dambiguousAutoLabels.insert(n->meta->anchorId);
        }
        drefTargets.insert(n->meta->anchorId, t);
    }
    for( int i = 0; i < n->children.size(); ++i)
        collectRefTargets(n->children[i]);
}

bool TypstGenerator::generate(const Node* doc, QTextStream& out, TypstGenError* err)
{
    if( !doc || doc->kind != Node::K_Document)
        return failAt(err, doc, "Root node is not a Document");

    dxrefStyle = doc->kv.value("attr:xrefstyle").trimmed();
    if( dxrefStyle.isEmpty())
        dxrefStyle = "short";
    drefTargets.clear();
    dautoLabels.clear();
    dambiguousAutoLabels.clear();
    dpartCount = 0;
    dnumbered = doc->kv.contains("attr:numbered") || doc->kv.contains("attr:sectnums");
    collectRefTargets(doc);

    if( !emitPreamble(doc, out, err))
        return false;

    const QString title = doc->kv.value("title");
    if( !title.isEmpty()) {
        out << "#align(center)[\n";
        out << "  #text(size: 20pt, weight: \"bold\")[" << escText(title) << "]\n";
        out << "]\n\n";
    }

    // emit TOC if :toc: attribute present; :toc: left/right are HTML hints,
    // paged output always places the TOC as front matter
    if( doc->kv.contains("attr:toc")) {
        int depth = 3;
        if( doc->kv.contains("attr:toclevels")) {
            bool ok = false;
            const int lv = doc->kv.value("attr:toclevels").trimmed().toInt(&ok);
            if( ok && lv >= 1 && lv <= 5)
                depth = lv;
        }
        out << "#outline(";
        if( doc->kv.contains("attr:toc-title"))
            out << "title: [" << escText(doc->kv.value("attr:toc-title").trimmed()) << "], ";
        out << "depth: " << depth << ")\n#pagebreak()\n\n";
    }

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
               "  width: 100%,\n"
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
               "  width: 100%,\n"
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

QString TypstGenerator::sectionLabel(const Node* n) const
{
    if( n->meta && !n->meta->anchorId.isEmpty())
        return labelSuffix(n->meta);
    const QString id = derivedSectionId(n->name);
    if( !id.isEmpty() && dautoLabels.contains(id))
        return " <" + id + ">";
    return QString();
}

bool TypstGenerator::emitSection(const Node* n, QTextStream& out, TypstGenError* err, int headingShift)
{
    int level = n->level + headingShift;
    if( level < 1 )
        level = 1;

    const bool isPart = hasStyle(n, "part");
    const bool isDiscrete = hasStyle(n, "discrete");

    if( isPart || isDiscrete) {
        // parts get their own Roman numbering; discrete headings are
        // unnumbered and (for discrete) excluded from the outline
        out << "#heading(level: " << level << ", numbering: none";
        if( isDiscrete)
            out << ", outlined: false";
        out << ")[";
        if( isPart)
            out << "Part " << romanNumeral(++dpartCount) << ": ";
        if( !n->titleChildren.isEmpty()) {
            if( !emitInlineSeq(n->titleChildren, out, err))
                return false;
        } else
            out << escText(n->name);
        out << "]" << sectionLabel(n) << "\n\n";
        for( int i=0;i<n->children.size();++i ) {
            if( !emitNode(n->children[i], out, err, headingShift))
                return false;
            out << "\n";
        }
        return true;
    }

    out << headingMarks(level) << " ";
    if( !n->titleChildren.isEmpty()) {
        if( !emitInlineSeq(n->titleChildren, out, err))
            return false;
    } else {
        out << escText(n->name);
    }
    out << sectionLabel(n) << "\n\n";
    for( int i=0;i<n->children.size();++i ) {
        if( !emitNode(n->children[i], out, err, headingShift))
            return false;
        out << "\n";
    }
    return true;
}

static const Node* soleImage(const Node* n)
{
    // a block-level image is a paragraph whose sole content is one image: macro
    const Node* img = 0;
    for( int i = 0; i < n->children.size(); ++i) {
        const Node* c = n->children[i];
        if( !c || c->kind == Node::K_Space)
            continue;
        if( c->kind == Node::K_InlineMacro && c->name == "image" && !img)
            img = c;
        else
            return 0; // other content present -> inline image, not a block figure
    }
    return img;
}

bool TypstGenerator::emitParagraph(const Node* n, QTextStream& out, TypstGenError* err)
{
    const Node* img = soleImage(n);
    if( img) {
        // standalone image: render as a centered figure, block title as caption
        const bool hasCaption = n->meta && !n->meta->title.isEmpty();
        out << "#figure(\n  [";
        emitImageCall(img->target.trimmed(), img->text, out);
        out << "],\n";
        if( hasCaption)
            out << "  caption: [" << escText(n->meta->title) << "],\n";
        out << ")" << labelSuffix(n->meta) << "\n";
        return true;
    }

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

/* A cols spec is either
   - relative widths ("1,2,3"),
   - alignment chars ("<,^,>"),
   - or a per-column combination ("<2,^1,>3").
*/
struct ColsInfo {
    QString columns; // Typst columns: argument and align
    QString align;
};

static QString alignName(const QString& a)
{
    if( a == "<")
        return "left";
    if( a == "^")
        return "center";
    if( a == ">")
        return "right";
    return "auto";
}

static ColsInfo parseColsSpec(const BlockMeta* m, int fallbackCols)
{
    ColsInfo ci;
    ci.columns = QString::number(fallbackCols);
    if( !m || !m->attrs.contains("cols"))
        return ci;

    QString v = m->attrs.value("cols");
    if( v.startsWith('"') && v.endsWith('"'))
        v = v.mid(1, v.size()-2);
    const QStringList parts = v.split(',');
    if( parts.isEmpty())
        return ci;

    QStringList widths;
    QStringList aligns;
    bool anyWidth = false;
    bool anyAlign = false;
    for( int i = 0; i < parts.size(); ++i) {
        QString p = parts[i].trimmed();
        QString al;
        // leading alignment char(s): <, ^, >
        while( !p.isEmpty() && (p[0] == '<' || p[0] == '^' || p[0] == '>')) {
            al = p.left(1);
            p = p.mid(1).trimmed();
        }
        bool ok = false;
        const int w = p.toInt(&ok);
        if( ok) {
            widths << (QString::number(w) + "fr");
            anyWidth = true;
        } else {
            widths << "1fr";
        }
        if( !al.isEmpty()) {
            anyAlign = true;
            aligns << alignName(al);
        } else {
            aligns << "auto";
        }
    }

    if( anyWidth)
        ci.columns = "(" + widths.join(", ") + ")";
    else
        ci.columns = QString::number(parts.size());

    if( anyAlign)
        ci.align = "(" + aligns.join(", ") + ")";

    return ci;
}

static QString imageDim(const QString& v)
{
    // convert an image dimension to a Typst length
    QString s = v.trimmed();
    if( s.isEmpty())
        return QString();
    if( s.endsWith('%'))
        return s;
    bool ok = false;
    const int n = s.toInt(&ok);
    if( ok && n > 0)
        return QString::number(n) + "pt";
    return QString();
}

void TypstGenerator::emitImageCall(const QString& path, const QString& attrs, QTextStream& out)
{
    // image attributes are positional (alt[,width[,height]]) but width/height
    // may also be given as named attributes (width=..., height=...).
    QString alt;
    QString width;
    QString height;
    const QStringList parts = attrs.split(',');
    int positional = 0;
    for( int i = 0; i < parts.size(); ++i) {
        const QString p = parts[i].trimmed();
        const int eq = p.indexOf('=');
        if( eq > 0) {
            const QString key = p.left(eq).trimmed();
            const QString val = p.mid(eq+1).trimmed();
            if( key == "width") width = val;
            else if( key == "height") height = val;
            else if( key == "alt") alt = val;
        } else {
            if( positional == 0) alt = p;
            else if( positional == 1) width = p;
            else if( positional == 2) height = p;
            ++positional;
        }
    }

    out << "#image(\"" << escString(path) << "\"";
    const QString w = imageDim(width);
    if( !w.isEmpty())
        out << ", width: " << w;
    const QString h = imageDim(height);
    if( !h.isEmpty())
        out << ", height: " << h;
    if( !alt.isEmpty())
        out << ", alt: \"" << escString(alt) << "\"";
    out << ")";
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

    const ColsInfo ci = parseColsSpec(n->meta, cols);
    out << "#table(columns: " << ci.columns;
    if( !ci.align.isEmpty())
        out << ", align: " << ci.align;
    out << ",\n";
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
    if( n->name == "include") {
        // include should have been resolved by preprocessor; skip if unresolved
        out << "// [unresolved include: " << escText(n->target) << "]\n";
        return true;
    }

    if( n->name == "image") {
        QString t = n->target.trimmed();
        int lb = t.indexOf('[');
        QString path = (lb < 0) ? t : t.left(lb).trimmed();
        QString attrs = (lb < 0) ? QString() : t.mid(lb+1, t.indexOf(']', lb) - lb - 1);
        emitImageCall(path, attrs, out);
        out << labelSuffix(n->meta) << "\n";
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
    // after preprocessing, remaining directives (e.g. unresolved ifdef) just emit body
    for( int i = 0; i < n->children.size(); ++i) {
        if( !emitNode(n->children[i], out, err, 0))
            return false;
    }
    return true;
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
    if( !n)
        return true;

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
            // bare <<id>>: honor reftext and :xrefstyle:
            const RefTarget t = drefTargets.value(n->target);
            if( t.numbered && dnumbered && dxrefStyle != "basic") {
                // short/full: Typst renders supplement + number (e.g. "Section 2.1")
                out << "#ref(<" << n->target << ">)";
                if( dxrefStyle == "full" && !t.text.isEmpty())
                    out << "#link(<" << n->target << ">)[, \u201C"
                        << escText(t.text) << "\u201D]";
            } else if( !t.text.isEmpty()) {
                out << "#link(<" << n->target << ">)[" << escText(t.text) << "]";
            } else {
                out << "#link(<" << n->target << ">)[" << escText(n->target) << "]";
            }
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
        if( n->name == "image") {
            emitImageCall(n->target.trimmed(), n->text, out);
            return true;
        }
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

