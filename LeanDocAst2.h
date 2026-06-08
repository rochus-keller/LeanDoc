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

struct RowCol {
    enum { ROW_BIT_LEN = 19, COL_BIT_LEN = 32 - ROW_BIT_LEN - 1, MSB = 0x80000000 };
    uint row : ROW_BIT_LEN; // supports 524k lines
    uint col : COL_BIT_LEN; // supports 4k chars per line
    uint unused : 1;
    RowCol(int l=0,int c=0):row(l),col(c){}
};

struct BlockMeta {
    QString anchorId;
    QString anchorText;
    QString title;
    QMap<QString, QString> attrs;
    QList<QString> roles;
    BlockMeta() {}
};

class Node {
public:
    enum Kind {
        K_Document,

        // block-level
        K_Section,
        K_Paragraph,
        K_LiteralParagraph,
        K_AdmonitionParagraph,
        K_DelimitedBlock,
        K_List,
        K_ListItem,
        K_Table,
        K_TableRow,
        K_TableCell,
        K_BlockMacro,
        K_Directive,
        K_ThematicBreak,
        K_PageBreak,
        K_LineComment,

        // inline-level
        K_Text,
        K_Space,
        K_LineBreak,
        K_Bold,
        K_Italic,
        K_Monospace,
        K_Highlight,
        K_Superscript,
        K_Subscript,
        K_Link,
        K_ImageInline,
        K_AnchorInline,
        K_Xref,
        K_AttrRef,
        K_InlineMacro,
        K_PassthroughInline
    };

    enum DelimKind { DK_None, DK_Listing, DK_Literal, DK_Quote,
                     DK_Example, DK_Sidebar, DK_Open, DK_Comment };
    enum ListType  { LT_None, LT_Unordered, LT_Ordered, LT_Description };
    enum CheckState { CS_None, CS_Unchecked, CS_Checked, CS_Intermediate };

    explicit Node(Kind k)
        : kind(k), pos(), meta(0),
          level(0), delimKind(DK_None), listType(LT_None), checkState(CS_None) {}
    virtual ~Node() {}

    Kind kind;
    RowCol pos;

    static const char* nodeKindName(Node::Kind k);
    const char* nodeKindName() const { return nodeKindName(kind); }
    void dump(QTextStream& out, int depth = 0);

    BlockMeta* meta;

    quint8 level;       // section depth (1..6), list marker level, desc term level
    quint8 delimKind;   // DelimKind for K_DelimitedBlock
    quint8 listType;    // ListType for K_List
    quint8 checkState;  // CheckState for K_ListItem

    QString text;       // raw content, literal text
    QString name;       // section title, macro name, admonition label, term text
    QString target;     // link/macro target or path
    QMap<QString, QString> kv; // document header attrs, dynamic attrs

    QList<Node*> children;
    void add(Node* n) { children.append(n); }

    static void deleteTree(Node* n) {
        if (!n) return;
        for (int i=0;i<n->children.size();++i)
            deleteTree(n->children[i]);
        delete n->meta;
        delete n;
    }
};

struct TableCellSpec {
    int colspan;
    int rowspan;
    QChar align;
    QChar style;
    TableCellSpec():colspan(1),rowspan(1),align(QChar()),style(QChar()){}
};

} // namespace LeanDoc

#endif
