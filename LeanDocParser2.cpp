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

#include "LeanDocParser2.h"
using namespace LeanDoc;

static inline bool FIRST_section(int k) {
    return k == LineTok::T_SECTION;
}

static inline bool FIRST_list(int k) {
    return k == LineTok::T_UL_ITEM || k == LineTok::T_OL_ITEM || k == LineTok::T_DESC_TERM;
}

static inline bool FIRST_delimited(int k) {
    return k == LineTok::T_DELIM_LISTING || k == LineTok::T_DELIM_LITERAL ||
           k == LineTok::T_DELIM_QUOTE   || k == LineTok::T_DELIM_EXAMPLE ||
           k == LineTok::T_DELIM_SIDEBAR  || k == LineTok::T_DELIM_OPEN ||
           k == LineTok::T_DELIM_COMMENT;
}

static inline bool FIRST_blockMeta(int k) {
    return k == LineTok::T_BLOCK_ANCHOR || k == LineTok::T_BLOCK_ATTRS || k == LineTok::T_BLOCK_TITLE;
}

static inline bool terminatesParagraph(int k) {
    return k == LineTok::T_BLANK || FIRST_section(k) || FIRST_list(k) ||
           k == LineTok::T_TABLE_DELIM || FIRST_delimited(k) ||
           k == LineTok::T_ADMONITION || k == LineTok::T_BLOCK_MACRO ||
           k == LineTok::T_DIRECTIVE || FIRST_blockMeta(k) ||
           k == LineTok::T_THEMATIC ||
           k == LineTok::T_LIST_CONT || k == LineTok::T_EOF;
}

// IDENTIFIER = ( ALPHA | "_" ) ( ALPHA | DIGIT | "_" | "-" )* ;
static bool isValidIdentifier(const QString& s)
{
    if( s.isEmpty())
        return false;
    if( !s[0].isLetter() && s[0] != '_')
        return false;
    for( int i = 1; i < s.size(); ++i) {
        const QChar c = s[i];
        if( !c.isLetterOrNumber() && c != '_' && c != '-')
            return false;
    }
    return true;
}

static int sectionLevel(const QString& raw) {
    const QString s = raw.trimmed();
    int n = 0;
    while( n < s.size() && s[n] == '=')
        ++n;
    return n;
}

static QString sectionTitle(const QString& raw) {
    const QString s = raw.trimmed();
    int n = 0;
    while( n < s.size() && s[n] == '=')
        ++n;
    return s.mid(n).trimmed();
}

static int markerLevel(const QString& raw, QChar marker) {
    const QString s = raw.trimmed();
    int n = 0;
    while( n < s.size() && s[n] == marker)
        ++n;
    return n;
}

static QString markerText(const QString& raw, QChar marker) {
    const QString s = raw.trimmed();
    int n = 0;
    while( n < s.size() && s[n] == marker)
        ++n;
    return s.mid(n).trimmed();
}

static bool matchAt(const QString& s, int pos, const char* pat, int patLen)
{
    if( pos + patLen > s.size()) return false;
    for( int k = 0; k < patLen; ++k)
        if( s[pos + k] != QLatin1Char(pat[k]))
            return false;
    return true;
}

static int findDelim(const QString& s, int start, const char* pat, int patLen)
{
    for( int i = start; i <= s.size() - patLen; ++i)
        if( matchAt(s, i, pat, patLen))
            return i;
    return -1;
}

struct InlineDelim {
    const char* open;
    int openLen;
    const char* close;
    int closeLen;
    Node::Kind kind;
    bool recurse;
};

static const InlineDelim inlineDelims[] = {
    { "**", 2, "**", 2, Node::K_Bold, true },
    { "__", 2, "__", 2, Node::K_Italic, true },
    { "``", 2, "``", 2, Node::K_Monospace, true },
    { "*",  1, "*",  1, Node::K_Bold, true },
    { "_",  1, "_",  1, Node::K_Italic, true },
    { "`",  1, "`",  1, Node::K_Monospace, false },
    { "#",  1, "#",  1, Node::K_Highlight, true },
    { "^",  1, "^",  1, Node::K_Superscript, false },
    { "~",  1, "~",  1, Node::K_Subscript, false }
};
static const int inlineDelimCount = (int)(sizeof(inlineDelims) / sizeof(inlineDelims[0]));

static bool isUrlSchemeStart(const QString& s, int i)
{
    return s.mid(i).startsWith("http://") || s.mid(i).startsWith("https://") ||
           s.mid(i).startsWith("ftp://")  || s.mid(i).startsWith("mailto:");
}

static bool isInlineMacroName(const QString& name)
{
    // kbd, btn, menu, pass unsupported
    return name == "image" || name == "link" || name == "mailto" || name == "xref" ||
           name == "footnote" || name == "latexmath" || name == "anchor";
}

static quint8 tokKindToDelimKind(LineTok::Kind k)
{
    switch( k ) {
    case LineTok::T_DELIM_LISTING:
        return Node::DK_Listing;
    case LineTok::T_DELIM_LITERAL:
        return Node::DK_Literal;
    case LineTok::T_DELIM_QUOTE:
        return Node::DK_Quote;
    case LineTok::T_DELIM_EXAMPLE:
        return Node::DK_Example;
    case LineTok::T_DELIM_SIDEBAR:
        return Node::DK_Sidebar;
    case LineTok::T_DELIM_OPEN:
        return Node::DK_Open;
    case LineTok::T_DELIM_COMMENT:
        return Node::DK_Comment;
    default:
        return Node::DK_None;
    }
}

// split a table row on unescaped '|', honouring \|
static QStringList splitUnescapedPipe(const QString& s)
{
    QStringList out;
    QString acc;
    for( int i = 0; i < s.size(); ++i ) {
        if( s[i] == '\\' && i + 1 < s.size() && s[i+1] == '|') {
            acc.append('|');
            ++i;
        } else if( s[i] == '|') {
            out.append(acc.trimmed());
            acc.clear();
        } else {
            acc.append(s[i]);
        }
    }
    out.append(acc.trimmed());
    return out;
}

bool Parser::accept(LineTok::Kind k)
{
    if( la(0).kind == k) {
        take();
        return true;
    }
    return false;
}

bool Parser::expect(LineTok::Kind k, const char* where)
{
    if( la(0).kind == k) {
        take();
        return true;
    }
    errors << Error(QString("expected %1 in %2").arg(LineTok::kindName(k)).arg(where), la(0).lineNo, 1);
    return false;
}

void Parser::error(const QString& msg, int row, int col)
{
    errors << Error(msg, row, col);
}

void Parser::skipBlankLines()
{
    while( la(0).kind == LineTok::T_BLANK )
        take();
}

QString Parser::stripOuter(const QString& s, QChar a, QChar b)
{
    if( s.size() >= 2 && s[0] == a && s[s.size()-1] == b )
        return s.mid(1, s.size()-2);
    return s;
}

// split on commas but respect quoted strings
static QStringList splitAttrComma(const QString& s)
{
    QStringList out;
    QString acc;
    bool inQuote = false;
    QChar quoteChar;
    for( int i = 0; i < s.size(); ++i ) {
        const QChar c = s[i];
        if( !inQuote && (c == '"' || c == '\'') ) {
            inQuote = true;
            quoteChar = c;
            acc.append(c);
        } else if( inQuote && c == quoteChar ) {
            inQuote = false;
            acc.append(c);
        } else if( !inQuote && c == ',' ) {
            out.append(acc);
            acc.clear();
        } else {
            acc.append(c);
        }
    }
    out.append(acc);
    return out;
}

QMap<QString, QString> Parser::parseAttrList(const QString& bracketed, int lineNo)
{
    QMap<QString, QString> res;
    const QString inner = stripOuter(bracketed.trimmed(), '[', ']');
    if( inner.isEmpty() )
        return res;
    const QStringList parts = splitAttrComma(inner);

    for( int i = 0; i < parts.size(); ++i ) {
        if( parts[i].trimmed().isEmpty() && parts.size() > 1)
            error("empty attribute entry (trailing, leading, or double comma)", lineNo);
    }

    int posIdx = 0;
    for( int i = 0; i < parts.size(); ++i ) {
        const QString p = parts[i].trimmed();
        if( p.isEmpty())
            continue;
        int eq = p.indexOf('=');
        if( eq > 0) {
            const QString key = p.left(eq).trimmed();
            // validate attribute key name
            if( !isValidIdentifier(key))
                error("invalid attribute key '" + key +
                      "'; must match IDENTIFIER (letter or underscore, then letters/digits/underscore/hyphen)", lineNo);
            res.insert(key, p.mid(eq+1).trimmed());
        } else if( posIdx == 0) {
            res.insert("positional0", p);
            ++posIdx;
        } else {
            res.insert(QString("positional%1").arg(posIdx), p);
            ++posIdx;
        }
    }
    return res;
}

Node* Parser::parse(const QString& input)
{
    errors.clear();
    dlex.setInput(input);
    return parseDocument();
}

Node* Parser::parseDocument()
{
    Node* doc = new Node(Node::K_Document);
    doc->pos = RowCol(1, 1);

    // skip leading comments and blanks before header
    while( la(0).kind == LineTok::T_BLANK || la(0).kind == LineTok::T_LINE_COMMENT) {
        if( la(0).kind == LineTok::T_LINE_COMMENT) {
            Node* c = new Node(Node::K_LineComment);
            c->pos = RowCol(la(0).lineNo, 1);
            c->text = la(0).raw.trimmed().mid(2);
            doc->add(c);
        }
        take();
    }
    parseDocumentHeader(doc);
    skipBlankLines();

    while( !dlex.atEnd()) {
        skipBlankLines();
        if( dlex.atEnd())
            break;
        Node* b = parseBlock();
        if( !b) break;
        doc->add(b);
    }
    return doc;
}

void Parser::parseDocumentHeader(Node* doc)
{
    // document title: = Title (level 1)
    if( la(0).kind == LineTok::T_SECTION && sectionLevel(la(0).raw) == 1) {
        LineTok t = take();
        doc->kv.insert("title", sectionTitle(t.raw));
        skipBlankLines();
    }

    // author line: Name <email>
    if( la(0).kind == LineTok::T_TEXT) {
        const QString s = la(0).raw.trimmed();
        if( s.contains('<') && s.contains('>')) {
            doc->kv.insert("authorLine", s);
            take();
            skipBlankLines();
        }
    }

    // revision line: vN.N, optional date/remark
    if( la(0).kind == LineTok::T_TEXT) {
        const QString s = la(0).raw.trimmed();
        if( s.size() >= 2 && s[0] == 'v' && s[1].isDigit()) {
            // validate: must be vDIGITS.DIGITS, optionally followed by ', date' or ', remark'
            int i = 1;
            while( i < s.size() && (s[i].isDigit() || s[i] == '.')) 
                ++i;
            if( i > 1 && (i >= s.size() || s[i] == ',' || s[i] == ' ')) {
                doc->kv.insert("revisionLine", s);
                take();
                skipBlankLines();
            } else {
                error("invalid revision line '" + s +
                      "'; expected format 'vN.N[, date][, remark]'", la(0).lineNo);
                take();
                skipBlankLines();
            }
        }
    }

    // document attributes: :name: value
    while( la(0).kind == LineTok::T_TEXT) {
        const QString s = la(0).raw.trimmed();
        if( !s.startsWith(':'))
            break;
        int second = s.indexOf(':', 1);
        if( second <= 1)
            break;
        const QString attrName = s.mid(1, second-1).trimmed();
        if( !isValidIdentifier(attrName))
            error("invalid document attribute name ':" + attrName +
                  ":'; must match IDENTIFIER", la(0).lineNo);
        const QString attrVal  = s.mid(second+1).trimmed();
        doc->kv.insert("attr:" + attrName, attrVal);
        take();
    }
}

Node* Parser::parseBlock()
{
    BlockMeta* meta = parseBlockMetaOpt();
    if( meta )
        skipBlankLines(); // block meta may be separated from its block by blank lines
    const int k = la(0).kind;

    if( meta && k == LineTok::T_EOF) {
        error("dangling block metadata (anchor/attributes/title) not followed by any block",
              meta->pos.row);
        delete meta;
        return 0;
    }

    if( FIRST_section(k))
        return parseSection(meta);
    if( k == LineTok::T_ADMONITION)
        return parseAdmonitionParagraph(meta);
    if( FIRST_list(k))
        return parseList(meta);
    if( k == LineTok::T_TABLE_DELIM)
        return parseTable(meta);
    if( FIRST_delimited(k))
        return parseDelimited(meta);
    if( k == LineTok::T_BLOCK_MACRO)
        return parseBlockMacro(meta);
    if( k == LineTok::T_DIRECTIVE)
        return parseDirective(meta);
    if( k == LineTok::T_THEMATIC || k == LineTok::T_LINE_COMMENT)
        return parseBreakOrComment(meta);
    if( k == LineTok::T_PAGEBREAK) {
        error("page break '<<<' is not supported by LeanDoc", la(0).lineNo);
        take();
        delete meta;
        return parseBlock();
    }
    if( k == LineTok::T_LIST_CONT) {
        error("list continuation '+' outside of a list context", la(0).lineNo);
        take();
        delete meta;
        return parseBlock();
    }
    if( k == LineTok::T_TEXT) {
        warnNearMissDelimiter(la(0).raw, la(0).lineNo);
        return parseParagraphOrLiteral(meta);
    }

    // unexpected token: skip and retry
    if( k != LineTok::T_EOF) {
        error("unexpected token", la(0).lineNo);
        take();
        delete meta;
        return parseBlock();
    }

    delete meta;
    return 0;
}

BlockMeta* Parser::parseBlockMetaOpt()
{
    if( !FIRST_blockMeta(la(0).kind)) return 0;

    BlockMeta* m = new BlockMeta();
    m->pos = RowCol(la(0).lineNo, 1);

    while( FIRST_blockMeta(la(0).kind)) {
        if( la(0).kind == LineTok::T_BLOCK_ANCHOR) {
            const int anchorLine = la(0).lineNo;
            const QString s = take().raw.trimmed();
            const QString inner = s.mid(2, s.size()-4);
            int comma = inner.indexOf(',');
            if( comma < 0 )
                m->anchorId = inner.trimmed();
            else {
                m->anchorId = inner.left(comma).trimmed();
                m->anchorText = inner.mid(comma+1).trimmed();
            }
            if( !m->anchorId.isEmpty() && !isValidIdentifier(m->anchorId))
                error("invalid anchor ID '" + m->anchorId +
                      "'; must match IDENTIFIER (start with letter/underscore, "
                      "then letters/digits/underscore/hyphen)", anchorLine);
        } else if( la(0).kind == LineTok::T_BLOCK_ATTRS) {
            const int attrLine = la(0).lineNo;
            const QString s = take().raw.trimmed();
            QMap<QString, QString> a = parseAttrList(s, attrLine);
            for( QMap<QString, QString>::ConstIterator it = a.constBegin(); it != a.constEnd(); ++it) {
                const QString& val = it.value();
                // handle AsciiDoc shorthand: [#id] [.role] [%option]
                if( it.key().startsWith("positional") && val.startsWith('#'))
                    m->anchorId = val.mid(1);
                else if( it.key().startsWith("positional") && val.startsWith('.'))
                    m->roles.append(val.mid(1));
                else if( it.key().startsWith("positional") && val.startsWith('%'))
                    m->attrs.insert("options", val.mid(1));
                else
                    m->attrs.insert(it.key(), val);
            }
        } else if( la(0).kind == LineTok::T_BLOCK_TITLE) {
            const QString s = take().raw.trimmed();
            m->title = s.mid(1).trimmed();
        }
    }

    return m;
}

Node* Parser::parseSection(BlockMeta* m)
{
    LineTok t = take();
    const int lvl = sectionLevel(t.raw);

    Node* n = new Node(Node::K_Section);
    n->pos = RowCol(t.lineNo, 1);
    n->meta = m;
    n->level = lvl;
    n->name = sectionTitle(t.raw);
    n->titleChildren = parseInlineContent(n->name, t.lineNo);

    while( !dlex.atEnd() ) {
        skipBlankLines();
        if( dlex.atEnd() )
            break;

        // stop if next section is same or higher level
        if( FIRST_section(la(0).kind)) {
            const int nextLvl = sectionLevel(la(0).raw);
            if( nextLvl <= lvl)
                break;
            if( nextLvl > lvl + 1)
                error("section level jumps from " + QString::number(lvl) +
                      " to " + QString::number(nextLvl) +
                      " (expected " + QString::number(lvl + 1) + ")", la(0).lineNo);
        }

        // peek past metadata to detect section end
        if( FIRST_blockMeta(la(0).kind)) {
            int off = 0;
            while( FIRST_blockMeta(la(off).kind))
                ++off;
            if( FIRST_section(la(off).kind) ) {
                const int nextLvl = sectionLevel(la(off).raw);
                if( nextLvl <= lvl)
                    break;
                // also check through metadata
                if( nextLvl > lvl + 1)
                    error("section level jumps from " + QString::number(lvl) +
                          " to " + QString::number(nextLvl) +
                          " (expected " + QString::number(lvl + 1) + ")", la(off).lineNo);
            }
        }

        if( la(0).kind == LineTok::T_TABLE_LINE) {
            error("unexpected table line outside table", la(0).lineNo);
            take();
            continue;
        }

        Node* b = parseBlock();
        if( !b )
            break;
        n->add(b);
    }
    return n;
}

Node* Parser::parseParagraphOrLiteral(BlockMeta* m)
{
    bool literal = (!la(0).raw.isEmpty() && la(0).raw[0].isSpace());

    Node* p = new Node(literal ? Node::K_LiteralParagraph : Node::K_Paragraph);
    p->pos = RowCol(la(0).lineNo, 1);
    p->meta = m;

    QStringList lines;
    while( la(0).kind == LineTok::T_TEXT) {
        if( literal) {
            if( la(0).raw.isEmpty() || !la(0).raw[0].isSpace() )
                break;
            lines << la(0).raw.mid(1);
        } else {
            const QString s = la(0).raw.trimmed();
            if( s.isEmpty())
                break;
            // hard line break: " +" at end of line
            if( s.endsWith(" +"))
                lines << s.left(s.size()-2) + "\n";
            else
                lines << s;
        }
        take();
        if( terminatesParagraph(la(0).kind))
            break;
    }

    if( literal)
        p->text = lines.join("\n");
    else
        p->children = parseInlineContent(lines.join(" "), p->pos.row);
    return p;
}

Node* Parser::parseAdmonitionParagraph(BlockMeta* m)
{
    LineTok t = take();
    const QString s = t.raw.trimmed();
    int colon = s.indexOf(':');

    Node* a = new Node(Node::K_AdmonitionParagraph);
    a->pos = RowCol(t.lineNo, 1);
    a->meta = m;
    a->name = s.left(colon);
    a->children = parseInlineContent(s.mid(colon+1).trimmed(), t.lineNo);
    return a;
}

Node* Parser::parseDelimited(BlockMeta* m)
{
    const LineTok::Kind k = la(0).kind;
    LineTok open = take();

    Node* b = new Node(Node::K_DelimitedBlock);
    b->pos = RowCol(open.lineNo, 1);
    b->meta = m;
    b->delimKind = tokKindToDelimKind(k);

    bool rawOnly = (k == LineTok::T_DELIM_LISTING || k == LineTok::T_DELIM_LITERAL ||
                    k == LineTok::T_DELIM_COMMENT);

    if( rawOnly) {
        QStringList lines;
        while( !dlex.atEnd() && la(0).kind != k)
            lines << take().raw;
        if( dlex.atEnd() || la(0).kind != k)
            error("unclosed delimited block ('" + open.raw.trimmed() +
                  "' opened at line " + QString::number(open.lineNo) + ")", la(0).lineNo);
        else
            take();
        b->text = lines.join("\n");
        return b;
    }

    // container block (quote, example, sidebar, open): parse children
    while( !dlex.atEnd() && la(0).kind != k) {
        skipBlankLines();
        if( la(0).kind == k || dlex.atEnd())
            break;
        Node* inner = parseBlock();
        if( !inner )
            break;
        b->add(inner);
    }
    if( dlex.atEnd() || la(0).kind != k)
        error("unclosed delimited block ('" + open.raw.trimmed() +
              "' opened at line " + QString::number(open.lineNo) + ")", la(0).lineNo);
    else
        take();
    return b;
}

Node* Parser::parseList(BlockMeta* m)
{
    Node* lst = new Node(Node::K_List);
    lst->pos = RowCol(la(0).lineNo, 1);
    lst->meta = m;

    if( la(0).kind == LineTok::T_DESC_TERM)
        lst->listType = Node::LT_Description;
    else if( la(0).kind == LineTok::T_OL_ITEM)
        lst->listType = Node::LT_Ordered;
    else
        lst->listType = Node::LT_Unordered;

    while( true) {
        if( lst->listType == Node::LT_Description) {
            if( la(0).kind != LineTok::T_DESC_TERM)
                break;

            LineTok termTok = take();
            const QString ts = termTok.raw.trimmed();
            int c = 0;
            for( int i = ts.size()-1; i >= 0 && ts[i] == ':'; --i)
                ++c;

            if( c < 2 || c > 4 )
                error("description list nesting depth " + QString::number(c) +
                      " out of range (2..4 colons allowed)", termTok.lineNo);

            Node* item = new Node(Node::K_ListItem);
            item->pos = RowCol(termTok.lineNo, 1);
            item->level = c;
            item->name = ts.left(ts.size()-c).trimmed();

            // optional definition on next line(s)
            if( la(0).kind == LineTok::T_TEXT && !la(0).raw.trimmed().isEmpty()) {
                LineTok defTok = la(0);
                QString defText = la(0).raw.trimmed();
                take();
                // collect wrapped continuation lines
                while( la(0).kind == LineTok::T_TEXT) {
                    const QString cont = la(0).raw.trimmed();
                    if( cont.isEmpty() )
                        break;
                    defText += " " + cont;
                    take();
                }
                Node* defPara = new Node(Node::K_Paragraph);
                defPara->pos = RowCol(defTok.lineNo, 1);
                defPara->children = parseInlineContent(defText, defTok.lineNo);
                item->add(defPara);
            }

            // list continuations (repeatable)
            skipBlankLines();
            while( la(0).kind == LineTok::T_LIST_CONT) {
                take();
                skipBlankLines();
                Node* cont = FIRST_delimited(la(0).kind)
                             ? parseDelimited(0)
                             : parseParagraphOrLiteral(0);
                if( cont )
                    item->add(cont);
                skipBlankLines();
            }

            lst->add(item);
            skipBlankLines();
            continue;
        }

        // ordered / unordered
        const bool ordered = (lst->listType == Node::LT_Ordered);
        if( ordered && la(0).kind != LineTok::T_OL_ITEM )
            break;
        if( !ordered && la(0).kind != LineTok::T_UL_ITEM)
            break;

        LineTok itTok = take();
        const QChar marker = ordered ? '.' : '*';
        const int lvl = markerLevel(itTok.raw, marker);
        QString payload = markerText(itTok.raw, marker);

        Node* item = new Node(Node::K_ListItem);
        item->pos = RowCol(itTok.lineNo, 1);
        item->level = lvl;

        // checklist detection
        if( payload.startsWith("[*]")) {
            item->checkState = Node::CS_Intermediate;
            payload = payload.mid(3).trimmed();
        } else if( payload.startsWith("[x]")) {
            item->checkState = Node::CS_Checked;
            payload = payload.mid(3).trimmed();
        } else if( payload.startsWith("[ ]")) {
            item->checkState = Node::CS_Unchecked;
            payload = payload.mid(3).trimmed();
        }

        // collect wrapped lines belonging to the same paragraph
        while( la(0).kind == LineTok::T_TEXT) {
            const QString cont = la(0).raw.trimmed();
            if( cont.isEmpty() )
                break;
            payload += " " + cont;
            take();
        }

        Node* headPara = new Node(Node::K_Paragraph);
        headPara->pos = RowCol(itTok.lineNo, 1);
        headPara->children = parseInlineContent(payload, itTok.lineNo);
        item->add(headPara);

        // list continuations (repeatable)
        skipBlankLines();
        while( la(0).kind == LineTok::T_LIST_CONT) {
            take();
            skipBlankLines();
            Node* cont = parseBlock();
            if( cont) item->add(cont);
            skipBlankLines();
        }

        lst->add(item);
        skipBlankLines();
    }

    return lst;
}

QList<Node*> Parser::readCells(const LineTok& rowTok)
{
    QList<Node*> cells;
    const QStringList parts = splitUnescapedPipe(rowTok.raw.trimmed());
    for( int i = 0; i < parts.size(); ++i) {
        if( parts[i].isEmpty() && i == 0 )
            continue;  // skip leading empty before first |
        Node* cell = new Node(Node::K_TableCell);
        cell->pos = RowCol(rowTok.lineNo, 1);
        cell->children = parseInlineContent(parts[i], rowTok.lineNo);
        cells.append(cell);
    }
    return cells;
}

static int colsCount(const BlockMeta* m)
{
    if( !m || !m->attrs.contains("cols"))
        return 0;
    QString v = m->attrs.value("cols");
    if( v.startsWith('"') && v.endsWith('"'))
        v = v.mid(1, v.size()-2);
    if( v.isEmpty())
        return 0;
    return v.split(',').size();
}

Node* Parser::parseTable(BlockMeta* m)
{
    LineTok open = la(0);
    if( !expect(LineTok::T_TABLE_DELIM, "table")) {
        delete m;
        return 0;
    }

    Node* t = new Node(Node::K_Table);
    t->pos = RowCol(open.lineNo, 1);
    t->meta = m;

    // collect all cells into a flat list; track first blank-line position
    // and max cells-per-line (for column count fallback)
    QList<Node*> allCells;
    int firstBlankPos = -1; // cell index at which first blank line after cells occurs
    int firstLineCells = 0; // cells produced by the first TABLE_LINE
    bool seenCell = false;

    while( !dlex.atEnd()) {
        if( la(0).kind == LineTok::T_TABLE_DELIM) {
            take();
            break;
        }
        if( la(0).kind == LineTok::T_BLANK) {
            if( seenCell && firstBlankPos < 0)
                firstBlankPos = allCells.size();
            take();
            continue;
        }
        if( la(0).kind == LineTok::T_TABLE_LINE) {
            QList<Node*> cells = readCells(la(0));
            if( !seenCell)
                firstLineCells = cells.size();
            allCells += cells;
            seenCell = true;
            take();
        } else {
            take();
        }
    }

    if( allCells.isEmpty())
        return t;

    // determine column count: cols attribute > header row boundary > first line cells
    int nCols = colsCount(m);
    if( nCols <= 0 && firstBlankPos > 0)
        nCols = firstBlankPos;
    if( nCols <= 0 && firstLineCells > 1)
        nCols = firstLineCells;
    if( nCols <= 0)
        nCols = allCells.size(); // single-row table

    if( allCells.size() % nCols != 0)
        error("cell count not evenly divisible by column count", open.lineNo);

    // group cells into rows
    const int nRows = nCols > 0 ? allCells.size() / nCols : 0;
    int off = 0;
    for( int r = 0; r < nRows; ++r) {
        Node* row = new Node(Node::K_TableRow);
        row->pos = allCells[off]->pos;
        for( int c = 0; c < nCols && off < allCells.size(); ++c)
            row->add(allCells[off++]);
        t->add(row);
    }

    // blank line after first row promotes it to header
    if( firstBlankPos == nCols)
        t->kv.insert("header", "true");

    return t;
}

Node* Parser::parseBlockMacro(BlockMeta* m)
{
    LineTok t = take();
    const QString s = t.raw.trimmed();
    int p = s.indexOf("::");

    Node* n = new Node(Node::K_BlockMacro);
    n->pos = RowCol(t.lineNo, 1);
    n->meta = m;
    n->name = s.left(p);
    n->target = s.mid(p+2);
    return n;
}

Node* Parser::parseDirective(BlockMeta* m)
{
    LineTok t = take();
    const QString s = t.raw.trimmed();
    int p = s.indexOf("::");

    Node* n = new Node(Node::K_Directive);
    n->pos = RowCol(t.lineNo, 1);
    n->meta = m;
    n->name = s.left(p);
    n->text = s.mid(p+2);

    if( n->name == "ifeval")
        error("'ifeval' directive is not supported by LeanDoc", t.lineNo);

    // ifdef/ifndef: collect body until endif::
    if( n->name == "ifdef" || n->name == "ifndef" || n->name == "ifeval") {
        while( !dlex.atEnd()) {
            skipBlankLines();
            if( la(0).kind == LineTok::T_DIRECTIVE) {
                const QString ds = la(0).raw.trimmed();
                if( ds.startsWith("endif::")) {
                    LineTok endTok = take();
                    int ep = ds.indexOf("::");
                    Node* endNode = new Node(Node::K_Directive);
                    endNode->pos = RowCol(endTok.lineNo, 1);
                    endNode->name = ds.left(ep);
                    endNode->text = ds.mid(ep+2);
                    n->add(endNode);
                    break;
                }
            }
            if( dlex.atEnd() )
                break;
            Node* body = parseBlock();
            if( !body)
                break;
            n->add(body);
        }
    }

    return n;
}

Node* Parser::parseBreakOrComment(BlockMeta* m)
{
    if( la(0).kind == LineTok::T_LINE_COMMENT) {
        LineTok t = take();
        Node* c = new Node(Node::K_LineComment);
        c->pos = RowCol(t.lineNo, 1);
        c->meta = m;
        c->text = t.raw.trimmed().mid(2);
        return c;
    }
    if( la(0).kind == LineTok::T_THEMATIC) {
        LineTok t = take();
        Node* b = new Node(Node::K_ThematicBreak);
        b->pos = RowCol(t.lineNo, 1);
        b->meta = m;
        return b;
    }
    if( la(0).kind == LineTok::T_PAGEBREAK) {
        LineTok t = take();
        Node* pb = new Node(Node::K_PageBreak);
        pb->pos = RowCol(t.lineNo, 1);
        pb->meta = m;
        return pb;
    }
    delete m;
    return 0;
}

QList<Node*> Parser::parseInlineContent(const QString& s, int lineNo)
{
    return parseInlineContentRec(s, lineNo, 0);
}

void Parser::pushText(QList<Node*>& out, const QString& t, int lineNo)
{
    if( t.isEmpty() )
        return;
    Node* n = new Node(Node::K_Text);
    n->pos = RowCol(lineNo, 1);
    n->text = t;
    out.append(n);
}

QList<Node*> Parser::parseInlineContentRec(const QString& s, int lineNo, int depth)
{
    QList<Node*> out;
    if( depth > 8) {
        pushText(out, s, lineNo);
        return out;
    }

    QString acc;
    int i = 0;
    while( i < s.size()) {

        // escape: \CHAR produces literal CHAR
        if( s[i] == '\\' && i + 1 < s.size()) {
            acc.append(s[i+1]);
            i += 2;
            continue;
        }

        // hard line break: embedded \n from " +" continuation
        if( s[i] == '\n') {
            pushText(out, acc, lineNo); acc.clear();
            Node* br = new Node(Node::K_LineBreak);
            br->pos = RowCol(lineNo, 1);
            out.append(br);
            ++i;
            continue;
        }

        // attribute reference {name}
        if( s[i] == '{') {
            int j = s.indexOf('}', i+1);
            if( j > i+1) {
                pushText(out, acc, lineNo); acc.clear();
                Node* ar = new Node(Node::K_AttrRef);
                ar->pos = RowCol(lineNo, 1);
                ar->name = s.mid(i+1, j-(i+1));
                out.append(ar);
                i = j + 1;
                continue;
            }
        }

        // cross-reference <<id,text>>
        if( matchAt(s, i, "<<", 2)) {
            int j = findDelim(s, i+2, ">>", 2);
            if( j > i+2) {
                pushText(out, acc, lineNo); acc.clear();
                Node* xr = new Node(Node::K_Xref);
                xr->pos = RowCol(lineNo, 1);
                const QString inner = s.mid(i+2, j-(i+2));
                int comma = inner.indexOf(',');
                if( comma < 0)
                    xr->target = inner.trimmed();
                else {
                    xr->target = inner.left(comma).trimmed();
                    xr->children = parseInlineContentRec(inner.mid(comma+1).trimmed(), lineNo, depth+1);
                }
                out.append(xr);
                i = j + 2;
                continue;
            }
        }

        // bibliography anchor [[[id]]] (must check before [[id]])
        if( matchAt(s, i, "[[[", 3)) {
            int j = findDelim(s, i+3, "]]]", 3);
            if( j > i+3) {
                pushText(out, acc, lineNo); acc.clear();
                Node* an = new Node(Node::K_AnchorInline);
                an->pos = RowCol(lineNo, 1);
                an->name = s.mid(i+3, j-(i+3));
                out.append(an);
                i = j + 3;
                continue;
            }
        }

        // inline anchor [[id]]
        if( matchAt(s, i, "[[", 2)) {
            int j = findDelim(s, i+2, "]]", 2);
            if( j > i+2) {
                pushText(out, acc, lineNo); acc.clear();
                Node* an = new Node(Node::K_AnchorInline);
                an->pos = RowCol(lineNo, 1);
                an->name = s.mid(i+2, j-(i+2));
                out.append(an);
                i = j + 2;
                continue;
            }
        }

        // URL autolink
        if( isUrlSchemeStart(s, i)) {
            pushText(out, acc, lineNo); acc.clear();
            int j = i;
            while( j < s.size() && !s[j].isSpace() && s[j] != '[')
                ++j;
            Node* lk = new Node(Node::K_Link);
            lk->pos = RowCol(lineNo, 1);
            lk->target = s.mid(i, j-i);
            // URL[text]
            if( j < s.size() && s[j] == '[') {
                int rb = s.indexOf(']', j+1);
                if( rb > j) {
                    lk->children = parseInlineContentRec(s.mid(j+1, rb-(j+1)), lineNo, depth+1);
                    j = rb + 1;
                }
            }
            out.append(lk);
            i = j;
            continue;
        }

        // inline macro: name:target[attrs]
        if( s[i].isLetter()) {
            int colon = s.indexOf(':', i);
            if( colon > i && colon + 1 < s.size() && s[colon+1] != ':' && s[colon+1] != ' ') {
                const QString macroName = s.mid(i, colon-i);

                if( isInlineMacroName(macroName)) {
                    int lb = s.indexOf('[', colon+1);

                    if( lb > colon && lb < s.size()) {
                        int rb = s.indexOf(']', lb+1);

                        if( rb > lb) {
                            pushText(out, acc, lineNo); acc.clear();
                            Node* mn = new Node(Node::K_InlineMacro);
                            mn->pos = RowCol(lineNo, 1);
                            mn->name = macroName;
                            mn->target = s.mid(colon+1, lb-(colon+1));
                            const QString inner = s.mid(lb+1, rb-(lb+1));
                            if( !inner.isEmpty())
                                mn->children = parseInlineContentRec(inner, lineNo, depth+1);
                            out.append(mn);
                            i = rb + 1;
                            continue;
                        }
                    }
                }
            }
        }

        // formatting delimiters (table-driven)
        {
            bool matched = false;
            for( int d = 0; d < inlineDelimCount; ++d) {
                const InlineDelim& dl = inlineDelims[d];

                if( matchAt(s, i, dl.open, dl.openLen)) {
                    int j = findDelim(s, i + dl.openLen, dl.close, dl.closeLen);

                    if( j > i + dl.openLen) {
                        pushText(out, acc, lineNo); acc.clear();
                        Node* n = new Node(dl.kind);
                        n->pos = RowCol(lineNo, 1);
                        const QString inner = s.mid(i + dl.openLen, j - (i + dl.openLen));
                        if( dl.recurse)
                            n->children = parseInlineContentRec(inner, lineNo, depth+1);
                        else
                            n->text = inner;
                        out.append(n);
                        i = j + dl.closeLen;
                        matched = true;
                        break;
                    }
                }
            }
            if( matched )
                continue;
        }

        // default: accumulate character
        acc.append(s[i]);
        ++i;
    }

    pushText(out, acc, lineNo);
    return out;
}

void Parser::warnNearMissDelimiter(const QString& raw, int lineNo)
{
    // warn on lines that look like delimiters but have wrong length
    const QString s = raw.trimmed();
    if( s.size() < 3)
        return;

    struct { QChar ch; const char* name; int exact; } pats[] = {
        { '-', "listing (----)", 4 },
        { '.', "literal (....)", 4 },
        { '_', "quote (____)", 4 },
        { '=', "example (====)", 4 },
        { '*', "sidebar (****)", 4 },
        { '/', "comment (////)", 4 }
    };
    const int npats = (int)(sizeof(pats) / sizeof(pats[0]));

    // check if entire line is one repeated char
    bool uniform = true;
    for( int i = 1; i < s.size(); ++i) {
        if( s[i] != s[0]) { uniform = false; break; }
    }
    if( !uniform)
        return;

    for( int p = 0; p < npats; ++p) {
        if( s[0] == pats[p].ch && s.size() != pats[p].exact && s.size() >= 3) {
            error("line of " + QString::number(s.size()) + " '" + s[0] +
                  "' characters looks like a " + pats[p].name +
                  " delimiter but has wrong length (expected exactly " +
                  QString::number(pats[p].exact) + ")", lineNo);
            return;
        }
    }
}
