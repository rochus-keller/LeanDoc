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

#include "LeanDocAst2.h"
using namespace LeanDoc;

const char* Node::nodeKindName(Kind k)
{
    switch (k) {
    case K_Document:
        return "Document";
    case K_Section:
        return "Section";
    case K_Paragraph:
        return "Paragraph";
    case K_LiteralParagraph:
        return "LiteralParagraph";
    case K_AdmonitionParagraph:
        return "AdmonitionParagraph";
    case K_DelimitedBlock:
        return "DelimitedBlock";
    case K_List:
        return "List";
    case K_ListItem:
        return "ListItem";
    case K_Table:
        return "Table";
    case K_TableRow:
        return "TableRow";
    case K_TableCell:
        return "TableCell";
    case K_BlockMacro:
        return "BlockMacro";
    case K_Directive:
        return "Directive";
    case K_ThematicBreak:
        return "ThematicBreak";
    case K_PageBreak:
        return "PageBreak";
    case K_LineComment:
        return "LineComment";
    case K_Text:
        return "Text";
    case K_Space:
        return "Space";
    case K_LineBreak:
        return "LineBreak";
    case K_Bold:
        return "Bold";
    case K_Italic:
        return "Italic";
    case K_Monospace:
        return "Monospace";
    case K_Highlight:
        return "Highlight";
    case K_Superscript:
        return "Superscript";
    case K_Subscript:
        return "Subscript";
    case K_Link:
        return "Link";
    case K_ImageInline:
        return "ImageInline";
    case K_AnchorInline:
        return "AnchorInline";
    case K_Xref:
        return "Xref";
    case K_AttrRef:
        return "AttrRef";
    case K_InlineMacro:
        return "InlineMacro";
    case K_PassthroughInline:
        return "PassthroughInline";
    default:
        return "Unknown";
    }
}

static void indent(QTextStream& out, int n)
{
    for (int i=0;i<n;i++)
        out << "  ";
}

static void dumpMeta(const BlockMeta* m, QTextStream& out)
{
    if( !m)
        return;
    if( !m->anchorId.isEmpty() )
        out << " anchorId=\"" << m->anchorId << "\"";
    if( !m->anchorText.isEmpty() )
        out << " anchorText=\"" << m->anchorText << "\"";
    if( !m->title.isEmpty() )
        out << " title=\"" << m->title << "\"";
    if( !m->attrs.isEmpty() )
        out << " attrs=" << m->attrs.size();
}

void Node::dump(QTextStream &out, int depth)
{
    indent(out, depth);
    out << nodeKindName() << " @" << pos.row;

    dumpMeta(meta, out);

    if( level )
        out << " level=" << level;
    if( delimKind )
        out << " delimKind=" << delimKind;
    if( listType )
        out << " listType=" << listType;
    if( checkState )
        out << " checkState=" << checkState;
    if( !name.isEmpty() )
        out << " name=\"" << name << "\"";
    if( !target.isEmpty() )
        out << " target=\"" << target << "\"";
    if( !text.isEmpty() ) {
        QString str = text;
        if( str.length() > 64) {
            str = str.simplified().left(64);
            str = "\"" + str + "\"...";
        } else
            str = "\"" + str + "\"";
        out << " text=" << str;
    }
    if( !kv.isEmpty() )
        out << " kv=" << kv.size();

    out << "\n";

    for( int i=0;i<children.size();++i )
        children[i]->dump(out, depth+1);
}
