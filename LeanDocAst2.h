#ifndef LEANDOC_AST2_H
#define LEANDOC_AST2_H

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

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QTextStream>

namespace LeanDoc {

struct SourcePos {
    int line;
    int column;
    SourcePos(int l=0,int c=0):line(l),column(c){}
};

struct BlockMeta {
    // From BlockMetadata grammar: [[id, text]] + [attrlist] + .Title [file:1]
    QString anchorId;
    QString anchorText;     // optional
    QString title;          // block title line (.Title)
    QMap<QString, QString> attrs; // parsed from [a=b,c=d] and also options/roles/etc.
    QList<QString> roles;   // derived convenience: ".role" etc. (still also in attrs)

    BlockMeta() {}
};

class Node {
public:
    enum Kind {
        K_Document,

        // Block-level
        K_Section,
        K_Paragraph,
        K_LiteralParagraph,
        K_AdmonitionParagraph,

        K_DelimitedBlock,        // listing/literal/quote/example/sidebar/open/passthrough/comment/stem
        K_List,                  // unordered/ordered/description
        K_ListItem,              // includes checklist state, term/definition, etc.
        K_Table,
        K_TableRow,
        K_TableCell,

        K_BlockMacro,            // image::, video::, audio::, include::, custom::target[]
        K_Directive,             // ifdef/ifndef/ifeval/endif (endif represented as part of directive node)
        K_ThematicBreak,
        K_PageBreak,
        K_LineComment,

        // Inline-level
        K_Text,
        K_Space,                 // optional: keep significant whitespace as nodes
        K_LineBreak,             // " <space>+<EOL>" [file:1]
        K_Emph,                  // bold/italic/mono/highlight/underline/etc.
        K_Superscript,
        K_Subscript,

        K_Link,                  // auto url, url[text], email, link:target[text]
        K_ImageInline,           // image:path[attrs]
        K_AnchorInline,          // [[id]] or [#id] in inline context
        K_Xref,                  // <<id,text>> or xref:id[text] [file:1]
        K_AttrRef,               // {name} and counter forms
        K_InlineMacro,           // kbd:/btn:/menu:/pass:/footnote:/indexterm:/stem:/custom [file:1]
        K_PassthroughInline      // +...+, ++...++, +++...+++ [file:1]
    };

    explicit Node(Kind k) : kind(k), pos(), meta(0) {}
    virtual ~Node() {}

    Kind kind;
    SourcePos pos;

    static const char* nodeKindName(Node::Kind k);
    const char* nodeKindName() const { return nodeKindName(kind); }

    // Block metadata applies to many block kinds; null when not applicable
    BlockMeta* meta;

    // Generic payload; interpretation depends on kind
    QString text;                  // e.g., paragraph raw, literal content, comment text, etc.
    QString name;                  // section title, macro name, admonition label, etc.
    QString target;                // link target, macro target/path, include path
    QMap<QString, QString> kv;     // attributes for macros/links/cells/options etc.

    QList<Node*> children;         // generic children (blocks or inline runs)
    void add(Node* n) { children.append(n); }

    // Convenience for cleanup
    static void deleteTree(Node* n) {
        if (!n)
            return;
        for (int i=0;i<n->children.size();++i)
            deleteTree(n->children[i]);
        delete n->meta;
        delete n;
    }
};

struct TableCellSpec {
    int colspan;    // ColSpan DIGIT+ [file:1]
    int rowspan;    // RowSpan .DIGIT+ [file:1]
    QChar align;    // '<' '^' '>' [file:1]
    QChar style;    // a,e,h,l,m,s,v [file:1]
    TableCellSpec():colspan(1),rowspan(1),align(QChar()),style(QChar()){}
};

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

} // namespace LeanDoc2

#endif

