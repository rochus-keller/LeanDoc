#include "LeanDocAst2.h"
using namespace LeanDoc;

inline const char *Node::nodeKindName(Kind k)
{
    switch (k) {
    case Node::K_Document: return "Document";
    case Node::K_Section: return "Section";
    case Node::K_Paragraph: return "Paragraph";
    case Node::K_LiteralParagraph: return "LiteralParagraph";
    case Node::K_AdmonitionParagraph: return "AdmonitionParagraph";
    case Node::K_DelimitedBlock: return "DelimitedBlock";
    case Node::K_List: return "List";
    case Node::K_ListItem: return "ListItem";
    case Node::K_Table: return "Table";
    case Node::K_TableRow: return "TableRow";
    case Node::K_TableCell: return "TableCell";
    case Node::K_BlockMacro: return "BlockMacro";
    case Node::K_Directive: return "Directive";
    case Node::K_ThematicBreak: return "ThematicBreak";
    case Node::K_PageBreak: return "PageBreak";
    case Node::K_LineComment: return "LineComment";
    case Node::K_Text: return "Text";
    case Node::K_Space: return "Space";
    case Node::K_LineBreak: return "LineBreak";
    case Node::K_Emph: return "Emph";
    case Node::K_Superscript: return "Superscript";
    case Node::K_Subscript: return "Subscript";
    case Node::K_Link: return "Link";
    case Node::K_ImageInline: return "ImageInline";
    case Node::K_AnchorInline: return "AnchorInline";
    case Node::K_Xref: return "Xref";
    case Node::K_AttrRef: return "AttrRef";
    case Node::K_InlineMacro: return "InlineMacro";
    case Node::K_PassthroughInline: return "PassthroughInline";
    default: return "Unknown";
    }
}


static void indent(QTextStream& out, int n)
{
    for (int i=0;i<n;i++)
        out << "  ";
}

static void dumpMeta(const BlockMeta* m, QTextStream& out)
{
    if (!m) return;
    if (!m->anchorId.isEmpty())
        out << " anchorId=\"" << m->anchorId << "\"";
    if (!m->anchorText.isEmpty())
        out << " anchorText=\"" << m->anchorText << "\"";
    if (!m->title.isEmpty())
        out << " title=\"" << m->title << "\"";
    if (!m->attrs.isEmpty())
        out << " attrs=" << m->attrs.size();
}

void Node::dump(QTextStream &out, int depth)
{
    indent(out, depth);
    out << nodeKindName() << " @" << pos.line;

    dumpMeta(meta, out);

    if (!name.isEmpty())
        out << " name=\"" << name << "\"";
    if (!target.isEmpty())
        out << " target=\"" << target << "\"";
    if (!text.isEmpty())
    {
        QString str = text;
        if( str.length() > 64 )
        {
            str = str.simplified().left(64);
            str = "\"" + str + "\"";
            str += "...";
        }else
            str = "\"" + str + "\"";
        out << " text=" << str;
    }
    if (!kv.isEmpty())
        out << " kv=" << kv.size();

    out << "\n";

    for (int i=0;i<children.size();++i) {
        children[i]->dump(out, depth+1);
    }
}
