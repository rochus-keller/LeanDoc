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

#include "LeanDocLexer2.h"

namespace LeanDoc {

const char* LineTok::kindName(Kind k)
{
    switch( k ) {
    case T_EOF:
        return "EOF";
    case T_BLANK:
        return "BLANK";
    case T_BLOCK_ANCHOR:
        return "BLOCK_ANCHOR";
    case T_BLOCK_ATTRS:
        return "BLOCK_ATTRS";
    case T_BLOCK_TITLE:
        return "BLOCK_TITLE";
    case T_SECTION:
        return "SECTION";
    case T_ADMONITION:
        return "ADMONITION";
    case T_LINE_COMMENT:
        return "LINE_COMMENT";
    case T_THEMATIC:
        return "THEMATIC";
    case T_PAGEBREAK:
        return "PAGEBREAK";
    case T_UL_ITEM:
        return "UL_ITEM";
    case T_OL_ITEM:
        return "OL_ITEM";
    case T_DESC_TERM:
        return "DESC_TERM";
    case T_LIST_CONT:
        return "LIST_CONT";
    case T_DELIM_LISTING:
        return "DELIM_LISTING";
    case T_DELIM_LITERAL:
        return "DELIM_LITERAL";
    case T_DELIM_QUOTE:
        return "DELIM_QUOTE";
    case T_DELIM_EXAMPLE:
        return "DELIM_EXAMPLE";
    case T_DELIM_SIDEBAR:
        return "DELIM_SIDEBAR";
    case T_DELIM_OPEN:
        return "DELIM_OPEN";
    case T_DELIM_COMMENT:
        return "DELIM_COMMENT";
    case T_TABLE_DELIM:
        return "TABLE_DELIM";
    case T_TABLE_LINE:
        return "TABLE_LINE";
    case T_BLOCK_MACRO:
        return "BLOCK_MACRO";
    case T_DIRECTIVE:
        return "DIRECTIVE";
    case T_TEXT:
        return "TEXT";
    default:
        return "UNKNOWN";
    }
}

void Lexer::setInput(const QString& text)
{
    dtoks.clear();
    dpos = 0;
    const QStringList lines = text.split('\n');
    for( int i = 0; i < lines.size(); ++i )
        dtoks.append(classify(lines[i], i + 1));
    LineTok eof;
    eof.kind = LineTok::T_EOF;
    eof.lineNo = lines.size() + 1;
    dtoks.append(eof);
}

const LineTok& Lexer::peek(int k) const
{
    int idx = dpos + k;
    if( idx < 0 )
        idx = 0;
    if( idx >= dtoks.size() )
        idx = dtoks.size() - 1;
    return dtoks[idx];
}

LineTok Lexer::take()
{
    const LineTok t = peek(0);
    if( dpos < dtoks.size() )
        ++dpos;
    return t;
}

bool Lexer::atEnd() const
{
    return peek(0).kind == LineTok::T_EOF;
}

static int leadingRun(const QString& s, QChar ch, int maxN)
{
    int n = 0;
    while( n < s.size() && n < maxN && s[n] == ch )
        ++n;
    return n;
}

LineTok Lexer::classify(const QString& line, int lineNo)
{
    LineTok t;
    t.lineNo = lineNo;
    t.raw = line;

    const QString s = line.trimmed();
    if( s.isEmpty() ) {
        t.kind = LineTok::T_BLANK;
        return t;
    }

    // block anchor [[id,...]]
    if( s.startsWith("[[") && s.endsWith("]]") ) {
        t.kind = LineTok::T_BLOCK_ANCHOR;
        return t;
    }

    // block attributes [...] (but not [[...]])
    if( s.size() >= 2 && s[0] == '[' && s[s.size()-1] == ']' && s[1] != '[' ) {
        t.kind = LineTok::T_BLOCK_ATTRS;
        return t;
    }

    // block title .Title (not multi-dot ordered list .., not delimiter ....)
    if( s.size() >= 2 && s[0] == '.' && s[1] != '.' && !s[1].isSpace() ) {
        t.kind = LineTok::T_BLOCK_TITLE;
        return t;
    }

    // directives
    if( s.startsWith("ifdef::") || s.startsWith("ifndef::") || s.startsWith("endif::") ) {
        t.kind = LineTok::T_DIRECTIVE;
        return t;
    }

    // description list: term followed by :: at end of line
    if( s.size() >= 3 && s.endsWith("::") ) {
        int c = 0;
        for (int i = s.size()-1; i >= 0 && s[i] == ':'; --i) ++c;
        if( c >= 2 && c < s.size()) {
            t.kind = LineTok::T_DESC_TERM;
            return t;
        }
    }

    // known block macros
    if( s.startsWith("include::") ) {
        t.kind = LineTok::T_BLOCK_MACRO;
        return t;
    }

    // line comment (but not //// comment delimiter)
    if( s.startsWith("//") && s != "////" ) {
        t.kind = LineTok::T_LINE_COMMENT;
        return t;
    }

    // thematic break
    if( s == "'''" ) {
        t.kind = LineTok::T_THEMATIC;
        return t;
    }

    // page break
    if( s.startsWith("<<<" )) {
        t.kind = LineTok::T_PAGEBREAK;
        return t;
    }

    // section title: =+ followed by space
    {
        int n = leadingRun(s, '=', 6);
        if( n >= 1 && n < s.size() && s[n].isSpace() ) {
            t.kind = LineTok::T_SECTION;
            return t;
        }
    }

    // unordered list: *+ followed by space
    {
        int n = leadingRun(s, '*', 6);
        if( n >= 1 && n < s.size() && s[n].isSpace() ) {
            t.kind = LineTok::T_UL_ITEM;
            return t;
        }
    }

    // ordered list: .+ followed by space
    {
        int n = leadingRun(s, '.', 6);
        if( n >= 1 && n < s.size() && s[n].isSpace() ) {
            t.kind = LineTok::T_OL_ITEM;
            return t;
        }
    }

    // list continuation
    if( s == "+" ) {
        t.kind = LineTok::T_LIST_CONT;
        return t;
    }

    // tables
    if( s == "|===" ) {
        t.kind = LineTok::T_TABLE_DELIM;
        return t;
    }
    if( s.startsWith("|") ) {
        t.kind = LineTok::T_TABLE_LINE;
        return t;
    }

    // delimited blocks (exactly 4 chars, or -- for open)
    if( s == "----") {
        t.kind = LineTok::T_DELIM_LISTING;
        return t;
    }
    if( s == "....") {
        t.kind = LineTok::T_DELIM_LITERAL;
        return t;
    }
    if( s == "____") {
        t.kind = LineTok::T_DELIM_QUOTE;
        return t;
    }
    if( s == "====") {
        t.kind = LineTok::T_DELIM_EXAMPLE;
        return t;
    }
    if( s == "****") {
        t.kind = LineTok::T_DELIM_SIDEBAR;
        return t;
    }
    if( s == "--")   {
        t.kind = LineTok::T_DELIM_OPEN;
        return t;
    }
    if( s == "////") {
        t.kind = LineTok::T_DELIM_COMMENT;
        return t;
    }

    // admonition paragraph
    if( s.startsWith("NOTE:") || s.startsWith("TIP:") || s.startsWith("IMPORTANT:") ||
        s.startsWith("CAUTION:") || s.startsWith("WARNING:") ) {
        t.kind = LineTok::T_ADMONITION;
        return t;
    }

    t.kind = LineTok::T_TEXT;
    return t;
}

} // namespace LeanDoc
