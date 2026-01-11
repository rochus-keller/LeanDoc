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
#include <QtDebug>

namespace LeanDoc {

static bool isUrlSchemeStart(const QString& s, int i)
{
    return s.mid(i).startsWith("http:") || s.mid(i).startsWith("https:") ||
           s.mid(i).startsWith("ftp:")  || s.mid(i).startsWith("irc:") ||
           s.mid(i).startsWith("mailto:");
}

Node* Parser::parse(const QString& input, ParseError* outErr)
{
    dlex.setInput(input);
    dhaveErr = false;
    derr = ParseError();

    Node* root = 0;
    try {
        root = parseDocument();
    }catch (...) {
        if (root)
            Node::deleteTree(root);
        root = 0;
    }

    if (outErr)
        *outErr = derr;
    return root;
}

bool Parser::accept(LineTok::Kind k)
{
    if (la(0).kind == k) {
        take();
        return true;
    }
    return false;
}

void Parser::expect(LineTok::Kind k, const QString& what)
{
    if (!accept(k)) {
        error("Expected " + what, la(0).lineNo);
    }
}

void Parser::error(const QString &msg, int row, int col)
{
    dhaveErr = true;
    derr.line = row;
    derr.column = col;
    derr.message = msg;
    throw 1;
}

void Parser::skipBlankAndLineComments()
{
    while (la(0).kind == LineTok::T_BLANK || la(0).kind == LineTok::T_LINE_COMMENT)
        take();
}

static QStringList splitUnescapedPipe(const QString& line)
{
    QStringList parts;
    QString cur;
    cur.reserve(line.size());

    int backslashRun = 0; // number of consecutive '\' immediately preceding current char

    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];

        if (c == '|') {
            if ((backslashRun % 2) == 0) {
                // true separator
                parts.append(cur);
                cur.clear();
            } else {
                // escaped pipe: replace "\|" with "|" (remove one escape backslash)
                if (!cur.isEmpty())
                    cur.chop(1);
                cur.append('|');
            }
            backslashRun = 0;
            continue;
        }

        cur.append(c);

        if (c == '\\')
            backslashRun += 1;
        else
            backslashRun = 0;
    }

    parts.append(cur);
    return parts;
}

QList<Node *> Parser::readCells(const LineTok & rowTok)
{
    // Split by '|' but keep empty prefix; cells are after each '|'
    QStringList parts = splitUnescapedPipe(rowTok.raw);
    QList<Node *> cells;
    for (int i=1;i<parts.size();++i) {
        QString cell = parts[i];

        Node* c = new Node(Node::K_TableCell);
        c->pos = SourcePos(rowTok.lineNo, 1);

        // CellSpec ends with '|' in grammar; in practice AsciiDoc spec is richer.
        // Implement a minimal subset: consume leading spec like "2.3+^a|" etc if present (best-effort).
        QString s = cell.trimmed();
        // For simplicity store raw cell text; advanced spec parsing can be extended.
        c->children = parseInlineContent(s, rowTok.lineNo);
        cells << c;
    }
    return cells;
}

QString Parser::stripOuter(const QString& s, QChar a, QChar b)
{
    QString t = s.trimmed();
    if (t.size() >= 2 && t[0]==a && t[t.size()-1]==b)
        return t.mid(1, t.size()-2);
    return t;
}

QMap<QString, QString> Parser::parseAttrList(const QString& bracketed)
{
    // Very small parser for AttributeEntry = IDENTIFIER [ "=" (IDENTIFIER | STRING_LITERAL) ]
    // Input can be "[...]" or the inner "..."
    QString inner = bracketed.trimmed();
    inner = stripOuter(inner, '[', ']');

    QMap<QString, QString> m;
    QString cur;
    QStringList parts = inner.split(',', QString::SkipEmptyParts);
    for (int i=0;i<parts.size();++i) {
        QString p = parts[i].trimmed();
        if (p.isEmpty())
            continue;
        int eq = p.indexOf('=');
        if (eq < 0) {
            m.insert(p, ""); // boolean attr
        } else {
            QString k = p.left(eq).trimmed();
            QString v = p.mid(eq+1).trimmed();
            // strip simple quotes "..."
            v = stripOuter(v, '\"', '\"');
            m.insert(k, v);
        }
    }
    return m;
}

Node* Parser::parseDocument()
{
    Node* doc = new Node(Node::K_Document);
    doc->pos = SourcePos(1,1);

    skipBlankAndLineComments();
    parseDocumentHeader(doc);

    while (!dlex.atEnd()) {
        skipBlankAndLineComments();
        if (dlex.atEnd())
            break;

        Node* b = parseBlock();
        if (b)
            doc->add(b);
        else
            break;
    }

    return doc;
}

void Parser::parseDocumentHeader(Node* doc)
{
    // Grammar: DocumentHeader = DocumentTitle [AuthorLine] [RevisionLine] AttributeEntry*
    // Store header fields in doc->kv for simplicity.
    if (la(0).kind == LineTok::T_SECTION && la(0).level == 1) {
        LineTok t = take();
        doc->kv.insert("title", t.rest);
        doc->kv.insert("titleLine", QString::number(t.lineNo));
        skipBlankAndLineComments();
    }

    // Minimal author line parsing: treat as raw if it looks like "Name <mail>" etc.
    if (la(0).kind == LineTok::T_TEXT) {
        QString s = la(0).raw.trimmed();
        if (s.contains('<') && s.contains('>')) {
            doc->kv.insert("authorLine", s);
            doc->kv.insert("authorLineNo", QString::number(la(0).lineNo));
            take();
            skipBlankAndLineComments();
        }
    }

    // Minimal revision line parsing: starts with 'v'
    if (la(0).kind == LineTok::T_TEXT) {
        QString s = la(0).raw.trimmed();
        if (s.startsWith("v")) {
            doc->kv.insert("revisionLine", s);
            doc->kv.insert("revisionLineNo", QString::number(la(0).lineNo));
            take();
            skipBlankAndLineComments();
        }
    }

    // Document attributes: :name: value
    while (la(0).kind == LineTok::T_TEXT) {
        QString s = la(0).raw.trimmed();
        if (!s.startsWith(":"))
            break;
        // parse ":name: value"
        int second = s.indexOf(':', 1);
        if (second <= 1)
            break;
        QString name = s.mid(1, second-1).trimmed();
        QString val  = s.mid(second+1).trimmed();
        doc->kv.insert("attr:" + name, val);
        take();
    }
}

BlockMeta* Parser::parseBlockMetaOpt()
{
    // BlockMetadata = [BlockAnchor] [BlockAttributes] [BlockTitle]
    if (la(0).kind != LineTok::T_BLOCK_ANCHOR &&
        la(0).kind != LineTok::T_BLOCK_ATTRS &&
        la(0).kind != LineTok::T_BLOCK_TITLE)
        return 0;

    BlockMeta* m = new BlockMeta();

    if (la(0).kind == LineTok::T_BLOCK_ANCHOR) {
        QString s = take().rest;         // like "[[id, text]]"
        QString inner = stripOuter(stripOuter(s, '[', ']'), '[', ']'); // remove both [[ ]]
        int comma = inner.indexOf(',');
        if (comma < 0)
            m->anchorId = inner.trimmed();
        else {
            m->anchorId = inner.left(comma).trimmed();
            m->anchorText = inner.mid(comma+1).trimmed();
        }
    }

    if (la(0).kind == LineTok::T_BLOCK_ATTRS) {
        QMap<QString, QString> a = parseAttrList(take().rest);
        for (QMap<QString, QString>::ConstIterator it=a.begin(); it!=a.end(); ++it)
            m->attrs.insert(it.key(), it.value());
        // derive roles: [.lead] style is encoded in key ".lead" or "role" depending on usage
        for (QMap<QString, QString>::ConstIterator it2=m->attrs.begin(); it2!=m->attrs.end(); ++it2) {
            if (it2.key().startsWith("."))
                m->roles.append(it2.key().mid(1));
        }
    }

    if (la(0).kind == LineTok::T_BLOCK_TITLE) {
        m->title = take().rest.trimmed();
    }

    // If nothing actually captured, still return it (presence matters for scoping).
    return m;
}

Node* Parser::parseBlock()
{
    BlockMeta* meta = parseBlockMetaOpt();

    // SpecialSection markers like [bibliography] are parsed as normal attributes in meta
    // and apply to the following Section block.
    if (la(0).kind == LineTok::T_SECTION)
        return parseSection(meta);
    if (la(0).kind == LineTok::T_ADMONITION)
        return parseAdmonitionParagraph(meta);

    if (la(0).kind == LineTok::T_UL_ITEM || la(0).kind == LineTok::T_OL_ITEM || la(0).kind == LineTok::T_DESC_TERM)
        return parseList(meta);

    if (la(0).kind == LineTok::T_TABLE_DELIM)
        return parseTable(meta);

    if (la(0).kind == LineTok::T_DELIM_LISTING || la(0).kind == LineTok::T_DELIM_LITERAL ||
        la(0).kind == LineTok::T_DELIM_QUOTE || la(0).kind == LineTok::T_DELIM_EXAMPLE ||
        la(0).kind == LineTok::T_DELIM_SIDEBAR || la(0).kind == LineTok::T_DELIM_OPEN ||
        la(0).kind == LineTok::T_DELIM_PASSTHROUGH || la(0).kind == LineTok::T_DELIM_COMMENT ||
        la(0).kind == LineTok::T_STEM_ATTR_LINE)
        return parseDelimited(meta);

    if (la(0).kind == LineTok::T_BLOCK_MACRO)
        return parseBlockMacro(meta);
    if (la(0).kind == LineTok::T_DIRECTIVE)
        return parseDirective(meta);

    if (la(0).kind == LineTok::T_THEMATIC || la(0).kind == LineTok::T_PAGEBREAK || la(0).kind == LineTok::T_LINE_COMMENT)
        return parseBreakOrComment(meta);

    // Paragraph (normal or literal)
    return parseParagraphOrLiteral(meta);
}

Node* Parser::parseSection(BlockMeta* m)
{
    LineTok t = take(); // T_SECTION
    Node* s = new Node(Node::K_Section);
    s->pos = SourcePos(t.lineNo, 1);
    s->meta = m;
    s->kv.insert("level", QString::number(t.level));
    s->name = t.rest; // title text (TEXT_RUN)
    // Consume until next section of same/higher level

    while (!dlex.atEnd()) {
        skipBlankAndLineComments();
        if (dlex.atEnd())
            break;
        const LineTok& cur = la(0);
        if (cur.kind == LineTok::T_SECTION && cur.level <= t.level)
            break;

        if (cur.kind == LineTok::T_TABLE_LINE )
            error("unexpected table line", cur.lineNo);

        const LineTok& next = la(1);
        if( (cur.kind == LineTok::T_BLOCK_ANCHOR || cur.kind == LineTok::T_BLOCK_ATTRS) &&
                next.kind == LineTok::T_SECTION && next.level <= t.level)
            break;

        Node* b = parseBlock();
        if (!b)
            break;
        s->add(b);
    }
    return s;
}

Node* Parser::parseAdmonitionParagraph(BlockMeta* m)
{
    LineTok t = take();
    Node* a = new Node(Node::K_AdmonitionParagraph);
    a->pos = SourcePos(t.lineNo, 1);
    a->meta = m;
    a->name = t.head; // NOTE/TIP/...
    a->children = parseInlineContent(t.rest, t.lineNo);
    return a;
}

Node* Parser::parseParagraphOrLiteral(BlockMeta* m)
{
    // LiteralParagraph starts with at least one space in the raw line
    bool literal = (la(0).kind == LineTok::T_TEXT && !la(0).raw.isEmpty() && la(0).raw[0].isSpace());

    Node* p = new Node(literal ?
                           Node::K_LiteralParagraph :
                           Node::K_Paragraph);
    p->pos = SourcePos(la(0).lineNo, 1);
    p->meta = m;

    QStringList lines;
    while (la(0).kind == LineTok::T_TEXT) {
        if (literal) {
            if (la(0).raw.isEmpty() || !la(0).raw[0].isSpace())
                break;
            lines << la(0).raw.mid(1); // keep verbatim minus the leading space
        } else {
            // stop paragraph on blank line or obvious block starters
            QString s = la(0).raw.trimmed();
            if (s.isEmpty())
                break;
            lines << s;
        }
        take();
        if (la(0).kind == LineTok::T_BLANK)
            break;
        if (!literal) {
            if (la(0).kind == LineTok::T_SECTION || la(0).kind == LineTok::T_UL_ITEM || la(0).kind == LineTok::T_OL_ITEM ||
                la(0).kind == LineTok::T_DESC_TERM || la(0).kind == LineTok::T_TABLE_DELIM ||
                la(0).kind == LineTok::T_DELIM_LISTING || la(0).kind == LineTok::T_DELIM_LITERAL ||
                la(0).kind == LineTok::T_ADMONITION || la(0).kind == LineTok::T_BLOCK_MACRO ||
                la(0).kind == LineTok::T_DIRECTIVE)
                break;
        }
    }

    if (literal) {
        p->text = lines.join("\n");
    } else {
        p->children = parseInlineContent(lines.join(" "), p->pos.line);
    }
    return p;
}

Node* Parser::parseDelimited(BlockMeta* m)
{
    // Handles: Listing/Literal/Quote/Example/Sidebar/Open/Passthrough/Comment, plus StemBlock
    // StemBlock grammar: [stem] then ++++ ... ++++
    bool isStem = false;
    if (la(0).kind == LineTok::T_STEM_ATTR_LINE) {
        take();
        isStem = true;
    }

    LineTok::Kind k = la(0).kind;
    LineTok open = take();

    Node* b = new Node(Node::K_DelimitedBlock);
    b->pos = SourcePos(open.lineNo, 1);
    b->meta = m;
    b->kv.insert("delim", QString::number((int)k));
    b->kv.insert("stem", isStem ? "1" : "0");

    // Listing/Literal/Passthrough/Comment/Stem are raw-ish; Quote/Example/Sidebar/Open contain BlockContent
    bool rawOnly = (k == LineTok::T_DELIM_LISTING || k == LineTok::T_DELIM_LITERAL ||
                    k == LineTok::T_DELIM_PASSTHROUGH || k == LineTok::T_DELIM_COMMENT || isStem);

    if (rawOnly) {
        QStringList lines;
        while (!dlex.atEnd() && la(0).kind != k) {
            lines << take().raw;
        }
        expect(k, "closing delimiter");
        b->text = lines.join("\n");
        return b;
    }

    // BlockContent = ( Paragraph | List | Table | DelimitedBlock | BLANK_LINE )*
    while (!dlex.atEnd() && la(0).kind != k) {
        skipBlankAndLineComments();
        if (la(0).kind == k)
            break;
        Node* inner = parseBlock();
        if (!inner)
            break;
        b->add(inner);
    }
    expect(k, "closing delimiter");
    return b;
}

Node* Parser::parseList(BlockMeta* m)
{
    Node* lst = new Node(Node::K_List);
    lst->pos = SourcePos(la(0).lineNo, 1);
    lst->meta = m;

    // Determine list type by first token
    if (la(0).kind == LineTok::T_DESC_TERM)
        lst->kv.insert("type", "description");
    else if (la(0).kind == LineTok::T_OL_ITEM)
        lst->kv.insert("type", "ordered");
    else
        lst->kv.insert("type", "unordered");

    while (true) {
        if (lst->kv.value("type")=="description") {
            if (la(0).kind != LineTok::T_DESC_TERM)
                break;
            LineTok termTok = take();

            Node* item = new Node(Node::K_ListItem);
            item->pos = SourcePos(termTok.lineNo, 1);
            item->kv.insert("kind", "definition");
            item->kv.insert("termLevel", QString::number(termTok.level));
            item->name = termTok.rest; // term text_run

            // optional definition line (InlineContent)
            if (la(0).kind == LineTok::T_TEXT && !la(0).raw.trimmed().isEmpty()) {
                QString defLine = la(0).raw.trimmed();
                take();
                Node* defPara = new Node(Node::K_Paragraph);
                defPara->pos = SourcePos(termTok.lineNo, 1);
                defPara->children = parseInlineContent(defLine, defPara->pos.line);
                item->add(defPara);
            }

            // ListItemContinuation: blank + + + blank + (Paragraph|DelimitedBlock)
            skipBlankAndLineComments();
            if (la(0).kind == LineTok::T_LIST_CONT) {
                take();
                skipBlankAndLineComments();
                Node* cont = (la(0).kind == LineTok::T_DELIM_LISTING || la(0).kind == LineTok::T_DELIM_LITERAL ||
                              la(0).kind == LineTok::T_DELIM_QUOTE   || la(0).kind == LineTok::T_DELIM_EXAMPLE ||
                              la(0).kind == LineTok::T_DELIM_SIDEBAR || la(0).kind == LineTok::T_DELIM_OPEN ||
                              la(0).kind == LineTok::T_DELIM_PASSTHROUGH || la(0).kind == LineTok::T_DELIM_COMMENT ||
                              la(0).kind == LineTok::T_STEM_ATTR_LINE)
                             ? parseDelimited(0)
                             : parseParagraphOrLiteral(0);
                if (cont)
                    item->add(cont);
            }

            lst->add(item);
            skipBlankAndLineComments();
            continue;
        }

        bool ordered = (lst->kv.value("type")=="ordered");
        if (ordered && la(0).kind != LineTok::T_OL_ITEM)
            break;
        if (!ordered && la(0).kind != LineTok::T_UL_ITEM)
            break;

        LineTok itTok = take();
        Node* item = new Node(Node::K_ListItem);
        item->pos = SourcePos(itTok.lineNo, 1);
        item->kv.insert("markerLevel", QString::number(itTok.level));

        // Checklist detection (simplified): starts with "[*]" "[x]" "[ ]"
        QString payload = itTok.rest;
        if (payload.startsWith("[*]") || payload.startsWith("[x]") || payload.startsWith("[ ]")) {
            item->kv.insert("check", payload.mid(1,1)); // '*','x',' '
            payload = payload.mid(3).trimmed();
        }

        Node* headPara = new Node(Node::K_Paragraph);
        headPara->pos = SourcePos(itTok.lineNo, 1);
        headPara->children = parseInlineContent(payload, itTok.lineNo);
        item->add(headPara);

        // Continuations (repeatable)
        skipBlankAndLineComments();
        while (la(0).kind == LineTok::T_LIST_CONT) {
            take();
            skipBlankAndLineComments();
            Node* cont = parseBlock();
            if (cont)
                item->add(cont);
            skipBlankAndLineComments();
        }

        lst->add(item);
        skipBlankAndLineComments();

        // NestedList = blank + List
        if (la(0).kind == LineTok::T_UL_ITEM || la(0).kind == LineTok::T_OL_ITEM || la(0).kind == LineTok::T_DESC_TERM) {
            // Let outer loop continue; nested structure can be reconstructed from markerLevel if needed.
        }
    }

    return lst;
}

static QStringList splitOnUnescapedPipe0(const QString& s)
{
    QStringList r; QString cur; int bs = 0;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (c == '|') {
            if (bs & 1) { cur.chop(1); cur += '|'; }  // "\|" -> "|"
            else { r << cur; cur.clear(); }
            bs = 0;
        } else {
            cur += c;
            bs = (c == '\\') ? (bs + 1) : 0;
        }
    }
    r << cur;
    return r;
}

Node* Parser::parseTable(BlockMeta* m)
{
    // Table = [TableAttributes] |=== content |===
    // We treat the opening |=== as required.
    LineTok open = la(0);
    expect(LineTok::T_TABLE_DELIM, "table delimiter |===");
    Node* t = new Node(Node::K_Table);
    t->pos = SourcePos(open.lineNo, 1);
    t->meta = m;

    // Parse rows until closing |=== (same token kind)
    QList< QList<Node*> > parts;
    while (!dlex.atEnd()) {
        if (la(0).kind == LineTok::T_TABLE_DELIM) {
            take();
            break;
        }
        if (la(0).kind == LineTok::T_BLANK) {
            take();
            continue;
        }
        if (la(0).kind != LineTok::T_TABLE_LINE) {
            take();
            continue;
        }

        LineTok rowTok = take(); 
        parts << readCells(rowTok);
    }
    // TODO: so far only basic table, table with header, and column spec are supported!
    if( !parts.isEmpty() )
    {
        const QList<Node*>& firstRow = parts.first();
        if( !firstRow.isEmpty() )
        {
            Node* row = new Node(Node::K_TableRow);
            row->pos = firstRow.first()->pos;
            for( int i = 0; i < firstRow.size(); i++ )
                row->add(firstRow[i]);
            t->add(row);
            QList<Node*> cells;
            for( int i = 1; i < parts.size(); i++ )
                cells += parts[i];
            if( cells.size() % firstRow.size() != 0 )
                error("the number of cells is not compatible with the table size", firstRow.first()->pos.line, firstRow.first()->pos.column);
            const int rows = cells.size() / firstRow.size();
            int off = 0;
            for( int r = 0; r < rows; r++ )
            {
                row = new Node(Node::K_TableRow);
                row->pos = cells[off]->pos;
                for( int i = 0; i < firstRow.size(); i++ )
                    row->add(cells[off++]);
                t->add(row);
            }
        }
    }

    return t;
}

Node* Parser::parseBlockMacro(BlockMeta* m)
{
    // BlockMacro grammar: image::/video::/audio::/include::/custom::target[attrs]
    LineTok t = take();
    Node* n = new Node(Node::K_BlockMacro);
    n->pos = SourcePos(t.lineNo, 1);
    n->meta = m;
    n->name = t.head;  // "image", "video", "audio", "include", or custom
    n->target = t.rest; // still contains "path[...]" portion
    return n;
}

Node* Parser::parseDirective(BlockMeta* m)
{
    // Directive grammar for ifdef/ifndef/ifeval with nested DocumentBody then endif::[]
    LineTok t = take();
    Node* n = new Node(Node::K_Directive);
    n->pos = SourcePos(t.lineNo, 1);
    n->meta = m;
    n->name = t.head;      // ifdef / ifndef / ifeval / endif

    // Minimal: store full directive tail for a later semantic pass.
    n->text = t.rest;

    // Parse body for ifdef/ifndef/ifeval until matching endif::[] (best-effort)
    if (n->name == "ifdef" || n->name == "ifndef" || n->name == "ifeval") {
        while (!dlex.atEnd()) {
            skipBlankAndLineComments();
            if (la(0).kind == LineTok::T_DIRECTIVE && la(0).raw.trimmed().startsWith("endif::")) {
                LineTok end = take();
                Node* endNode = new Node(Node::K_Directive);
                endNode->pos = SourcePos(end.lineNo, 1);
                endNode->name = "endif";
                endNode->text = end.rest;
                n->add(endNode);
                break;
            }
            if (dlex.atEnd())
                break;
            Node* b = parseBlock();
            if (!b)
                break;
            n->add(b);
        }
    }

    return n;
}

Node* Parser::parseBreakOrComment(BlockMeta* m)
{
    if (la(0).kind == LineTok::T_LINE_COMMENT) {
        LineTok t = take();
        Node* c = new Node(Node::K_LineComment);
        c->pos = SourcePos(t.lineNo, 1);
        c->meta = m;
        c->text = t.rest;
        return c;
    }
    if (la(0).kind == LineTok::T_THEMATIC) {
        LineTok t = take();
        Node* b = new Node(Node::K_ThematicBreak);
        b->pos = SourcePos(t.lineNo, 1);
        b->meta = m;
        b->text = t.raw.trimmed();
        return b;
    }
    if (la(0).kind == LineTok::T_PAGEBREAK) {
        LineTok t = take();
        Node* p = new Node(Node::K_PageBreak);
        p->pos = SourcePos(t.lineNo, 1);
        p->meta = m;
        p->text = t.rest;
        return p;
    }
    // fallback
    delete m;
    return 0;
}

// -------- Inline parsing (greedy, recursive) --------

void Parser::pushText(QList<Node*>& out, const QString& t, int lineNo)
{
    if (t.isEmpty())
        return;
    Node* n = new Node(Node::K_Text);
    n->pos = SourcePos(lineNo, 1);
    n->text = t;
    out.append(n);
}

QList<Node*> Parser::parseInlineContent(const QString& s, int lineNo)
{
    return parseInlineContentRec(s, lineNo, 0);
}

QList<Node*> Parser::parseInlineContentRec(const QString& s, int lineNo, int depth)
{
    // This is a pragmatic full-coverage scanner: it recognizes the inline element families
    // and preserves everything else as text.
    QList<Node*> out;
    QString acc;

    int i=0;
    while (i < s.size()) {
        // Attribute reference: {name}
        if (s[i] == '{') {
            int j = s.indexOf('}', i+1);
            if (j > i+1) {
                pushText(out, acc, lineNo); acc.clear();
                Node* n = new Node(Node::K_AttrRef);
                n->pos = SourcePos(lineNo, 1);
                n->name = s.mid(i+1, j-i-1).trimmed();
                out.append(n);
                i = j+1;
                continue;
            }
        }

        // Cross reference: <<id,text>>
        if (i+1 < s.size() && s[i]=='<' && s[i+1]=='<') {
            int j = s.indexOf(">>", i+2);
            if (j > i+2) {
                pushText(out, acc, lineNo); acc.clear();
                QString inner = s.mid(i+2, j-(i+2));
                int comma = inner.indexOf(',');
                Node* x = new Node(Node::K_Xref);
                x->pos = SourcePos(lineNo, 1);
                if (comma < 0) {
                    x->target = inner.trimmed();
                } else {
                    x->target = inner.left(comma).trimmed();
                    x->children = parseInlineContentRec(inner.mid(comma+1).trimmed(), lineNo, depth+1);
                }
                out.append(x);
                i = j+2;
                continue;
            }
        }

        // Inline anchor: [[id,...]]
        if (i+1 < s.size() && s[i]=='[' && s[i+1]=='[') {
            int j = s.indexOf("]]", i+2);
            if (j > i+2) {
                pushText(out, acc, lineNo); acc.clear();
                QString inner = s.mid(i+2, j-(i+2));
                int comma = inner.indexOf(',');
                Node* a = new Node(Node::K_AnchorInline);
                a->pos = SourcePos(lineNo, 1);
                if (comma < 0)
                    a->name = inner.trimmed();
                else {
                    a->name = inner.left(comma).trimmed();
                    a->children = parseInlineContentRec(inner.mid(comma+1).trimmed(), lineNo, depth+1);
                }
                out.append(a);
                i = j+2;
                continue;
            }
        }

        // URL auto link (scheme + path)
        if (isUrlSchemeStart(s, i)) {
            // consume until whitespace or '[' or ']'
            int j=i;
            while (j<s.size() && !s[j].isSpace() && s[j] != '[' && s[j] != ']')
                ++j;
            if (j > i+5) {
                pushText(out, acc, lineNo); acc.clear();
                Node* l = new Node(Node::K_Link);
                l->pos = SourcePos(lineNo, 1);
                l->target = s.mid(i, j-i);
                out.append(l);
                i = j;
                continue;
            }
        }

        // Inline macro: name:...[...] (best-effort)
        {
            int colon = s.indexOf(':', i);
            if (colon == i) { /* ignore */ }
            if (colon > i) {
                // potential name
                bool ok = true;
                for (int k=i;k<colon;++k) {
                    QChar ch = s[k];
                    if (!(ch.isLetterOrNumber() || ch=='_' || ch=='-')) {
                        ok=false;
                        break;
                    }
                }
                if (ok && colon+1 < s.size()) {
                    int lb = s.indexOf('[', colon+1);
                    int rb = s.indexOf(']', lb+1);
                    if (lb > colon && rb > lb) {
                        pushText(out, acc, lineNo); acc.clear();
                        Node* m = new Node(Node::K_InlineMacro);
                        m->pos = SourcePos(lineNo, 1);
                        m->name = s.mid(i, colon-i);
                        // target is the segment after ":" up to "[" (can be empty for kbd:[...])
                        m->target = s.mid(colon+1, lb-(colon+1));
                        QString inner = s.mid(lb+1, rb-(lb+1));
                        m->children = parseInlineContentRec(inner, lineNo, depth+1);
                        out.append(m);
                        i = rb+1;
                        continue;
                    }
                }
            }
        }

        // Formatting / passthrough: recognize pairs/triples first
        // **...**, __...__, ``...``, +++...+++, ++...++, +...+, #...#, ^...^, ~...~
        struct Del {
            QString open;
            QString close;
            Node::Kind kind;
        };
        Del dels[] = {
            { "***", "***", Node::K_PassthroughInline }, // not in grammar; kept as passthrough safeguard
        };
        Q_UNUSED(dels);

        // **bold** (unconstrained)
        if (s.mid(i,2)=="**") {
            int j = s.indexOf("**", i+2);
            if (j > i+2) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Emph);
                e->pos = SourcePos(lineNo, 1);
                e->name = "bold";
                e->children = parseInlineContentRec(s.mid(i+2, j-(i+2)), lineNo, depth+1);
                out.append(e);
                i = j+2;
                continue;
            }
        }
        // *bold* (constrained)
        if (s[i]=='*') {
            int j = s.indexOf('*', i+1);
            if (j > i+1) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Emph);
                e->pos = SourcePos(lineNo, 1);
                e->name = "bold";
                e->children = parseInlineContentRec(s.mid(i+1, j-(i+1)), lineNo, depth+1);
                out.append(e);
                i = j+1;
                continue;
            }
        }
        // __italic__ (unconstrained)
        if (s.mid(i,2)=="__") {
            int j = s.indexOf("__", i+2);
            if (j > i+2) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Emph);
                e->pos = SourcePos(lineNo, 1);
                e->name = "italic";
                e->children = parseInlineContentRec(s.mid(i+2, j-(i+2)), lineNo, depth+1);
                out.append(e);
                i = j+2;
                continue;
            }
        }
        // _italic_ (constrained)
        if (s[i]=='_') {
            int j = s.indexOf('_', i+1);
            if (j > i+1) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Emph);
                e->pos = SourcePos(lineNo, 1);
                e->name = "italic";
                e->children = parseInlineContentRec(s.mid(i+1, j-(i+1)), lineNo, depth+1);
                out.append(e);
                i = j+1;
                continue;
            }
        }
        // ``mono`` (unconstrained)
        if (s.mid(i,2)=="``") {
            int j = s.indexOf("``", i+2);
            if (j > i+2) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Emph);
                e->pos = SourcePos(lineNo, 1);
                e->name = "mono";
                e->children = parseInlineContentRec(s.mid(i+2, j-(i+2)), lineNo, depth+1);
                out.append(e);
                i = j+2;
                continue;
            }
        }
        // `mono` (constrained)
        if (s[i]=='`') {
            int j = s.indexOf('`', i+1);
            if (j > i+1) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Emph);
                e->pos = SourcePos(lineNo, 1);
                e->name = "mono";
                e->text = s.mid(i+1, j-(i+1)); // keep as raw text run
                out.append(e);
                i = j+1;
                continue;
            }
        }
        // #highlight#
        if (s[i]=='#') {
            int j = s.indexOf('#', i+1);
            if (j > i+1) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Emph);
                e->pos = SourcePos(lineNo, 1);
                e->name = "highlight";
                e->children = parseInlineContentRec(s.mid(i+1, j-(i+1)), lineNo, depth+1);
                out.append(e);
                i = j+1;
                continue;
            }
        }
        // ^sup^
        if (s[i]=='^') {
            int j = s.indexOf('^', i+1);
            if (j > i+1) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Superscript);
                e->pos = SourcePos(lineNo, 1);
                e->text = s.mid(i+1, j-(i+1));
                out.append(e);
                i = j+1;
                continue;
            }
        }
        // ~sub~
        if (s[i]=='~') {
            int j = s.indexOf('~', i+1);
            if (j > i+1) {
                pushText(out, acc, lineNo); acc.clear();
                Node* e = new Node(Node::K_Subscript);
                e->pos = SourcePos(lineNo, 1);
                e->text = s.mid(i+1, j-(i+1));
                out.append(e);
                i = j+1;
                continue;
            }
        }
        // passthrough: +...+ / ++...++ / +++...+++ (nested parsing allowed by grammar)
        if (s[i]=='+') {
            int plusN=1;
            while (i+plusN<s.size() && s[i+plusN]=='+')
                ++plusN;
            if (plusN>=1 && plusN<=3) {
                QString fence(plusN, '+');
                int j = s.indexOf(fence, i+plusN);
                if (j > i+plusN) {
                    pushText(out, acc, lineNo); acc.clear();
                    Node* p = new Node(Node::K_PassthroughInline);
                    p->pos = SourcePos(lineNo, 1);
                    p->kv.insert("plusN", QString::number(plusN));
                    p->children = parseInlineContentRec(s.mid(i+plusN, j-(i+plusN)), lineNo, depth+1);
                    out.append(p);
                    i = j+plusN;
                    continue;
                }
            }
        }

        // default char
        acc.append(s[i]);
        ++i;
    }

    pushText(out, acc, lineNo);
    return out;
}

} // namespace LeanDoc2


