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

#include "LeanDocValidator.h"
using namespace LeanDoc;

static bool isValidIdentifier(const QString& s)
{
    if( s.isEmpty() )
        return false;
    if( !s[0].isLetter() && s[0] != '_')
        return false;
    for( int i = 1; i < s.size(); ++i) {
        const QChar c = s[i];
        if( !c.isLetterOrNumber() && c != '_' && c != '-' )
            return false;
    }
    return true;
}

static int parseColsCount(const QString& raw)
{
    QString v = raw.trimmed();
    if( v.startsWith('"') && v.endsWith('"'))
        v = v.mid(1, v.size()-2);
    if( v.isEmpty())
        return 0;
    return v.split(',').size();
}

void Validator::validate(const Node* doc)
{
    diagnostics.clear();
    danchors.clear();
    danchorLines.clear();

    if( !doc || doc->kind != Node::K_Document)
        return;

    collectAnchors(doc);

    // check attributes, references, context, etc.
    checkNode(doc);
}

void Validator::collectAnchors(const Node* n)
{
    if( !n )
        return;

    // explicit block anchor from metadata
    if( n->meta && !n->meta->anchorId.isEmpty()) {
        const QString& id = n->meta->anchorId;
        const int line = n->meta->pos.row;

        if( !isValidIdentifier(id))
            error(line, "invalid anchor ID '[[" + id +
                  "]]'; must match IDENTIFIER (start with letter/underscore, "
                  "then letters/digits/underscore/hyphen)");

        if( danchors.contains(id))
            error(line, "duplicate anchor ID '[[" + id +
                  "]]' (first declared at line " +
                  QString::number(danchorLines.value(id)) + ")");
        else {
            danchors.insert(id);
            danchorLines.insert(id, line);
        }
    }

    // inline anchor (stored in name field)
    if( n->kind == Node::K_AnchorInline && !n->name.isEmpty()) {
        const QString& id = n->name;
        const int line = n->pos.row;

        if( !isValidIdentifier(id))
            error(line, "invalid inline anchor ID '" + id +
                  "'; must match IDENTIFIER");

        if( danchors.contains(id))
            error(line, "duplicate anchor ID '" + id +
                  "' (first declared at line " +
                  QString::number(danchorLines.value(id)) + ")");
        else {
            danchors.insert(id);
            danchorLines.insert(id, line);
        }
    }

    for( int i = 0; i < n->children.size(); ++i)
        collectAnchors(n->children[i]);
}

void Validator::checkNode(const Node* n)
{
    if( !n )
        return;

    switch( n->kind ) {
    case Node::K_Table:
        checkTableAttrs(n);
        checkTableCellCount(n);
        break;
    case Node::K_Xref:
        checkXref(n);
        break;
    case Node::K_DelimitedBlock:
        checkBlockAttrs(n);
        checkSourceAttrContext(n);
        checkAdmonitionAttrContext(n);
        break;
    case Node::K_List:
        checkBlockAttrs(n);
        checkColsOnNonTable(n);
        break;
    case Node::K_Paragraph:
        checkBlockAttrs(n);
        checkColsOnNonTable(n);
        break;
    case Node::K_Section:
        checkBlockAttrs(n);
        checkRoleContext(n);
        checkColsOnNonTable(n);
        break;
    case Node::K_AdmonitionParagraph:
        checkBlockAttrs(n);
        checkColsOnNonTable(n);
        break;
    default:
        break;
    }

    for( int i = 0; i < n->children.size(); ++i )
        checkNode(n->children[i]);
    for( int i = 0; i < n->titleChildren.size(); ++i )
        checkNode(n->titleChildren[i]);
}

static bool isValidColSpec(const QString& raw)
{
    const QString s = raw.trimmed();
    QString inner = s;
    if( inner.startsWith('"') && inner.endsWith('"'))
        inner = inner.mid(1, inner.size()-2);
    if( inner.isEmpty())
        return false;

    const QStringList parts = inner.split(',');
    for( int i = 0; i < parts.size(); ++i) {
        const QString p = parts[i].trimmed();
        if( p.isEmpty() )
            return false; // empty entry (trailing/double comma)

        if( p.contains('%') )
            return false;

        bool hasDigit = false;
        bool hasAlign = false;
        for( int j = 0; j < p.size(); ++j) {
            const QChar c = p[j];
            if( c.isDigit()) {
                if( hasAlign )
                    return false;
                hasDigit = true;
            } else if( c == '<' || c == '^' || c == '>') {
                hasAlign = true;
            } else {
                return false;
            }
        }
        if( !hasDigit && !hasAlign )
            return false;
    }
    return true;
}

// known table options per spec
static bool isKnownTableOption(const QString& opt)
{
    return opt == "header" || opt == "footer" || opt == "autowidth";
}

void Validator::checkTableAttrs(const Node* n)
{
    if( !n->meta )
        return;

    const QMap<QString, QString>& a = n->meta->attrs;
    for( QMap<QString, QString>::ConstIterator it = a.constBegin(); it != a.constEnd(); ++it) {
        const QString& key = it.key();

        if( key == "width") {
            error(n->pos.row, "unsupported table attribute 'width'; "
                  "table width is determined by the rendering backend");
        } else if( key == "cols") {
            if( !isValidColSpec(it.value()))
                error(n->pos.row, "invalid 'cols' format '" + it.value() +
                      "'; expected relative widths (e.g. \"1,2,3\") "
                      "or alignment (e.g. \"<,^,>\")");
        } else if( key == "options") {
            const QString v = it.value().trimmed();
            QString inner = v;
            if( inner.startsWith('"') && inner.endsWith('"'))
                inner = inner.mid(1, inner.size()-2);
            const QStringList opts = inner.split(',');
            for( int i = 0; i < opts.size(); ++i) {
                const QString opt = opts[i].trimmed();
                if( !opt.isEmpty() && !isKnownTableOption(opt))
                    error(n->pos.row, "unknown table option '" + opt + "'");
            }
        } else if( key.startsWith("positional")) {
            // positional args: first is typically a block role; ignore for tables
        } else if( key == "id" || key == "role") {
            // valid general attributes
        } else {
            error(n->pos.row, "unknown table attribute '" + key + "'");
        }
    }
}

void Validator::checkTableCellCount(const Node* n)
{
    if( !n->meta || !n->meta->attrs.contains("cols"))
        return;

    // cross-check cols count vs actual table cell count
    const int specCols = parseColsCount(n->meta->attrs.value("cols"));
    if( specCols <= 0)
        return;

    // count total cells and check each row
    for( int i = 0; i < n->children.size(); ++i) {
        const Node* row = n->children[i];
        if( !row || row->kind != Node::K_TableRow)
            continue;
        const int rowCols = row->children.size();
        if( rowCols != specCols)
            error(row->pos.row, "table row has " + QString::number(rowCols) +
                  " cells but cols attribute specifies " +
                  QString::number(specCols) + " columns");
    }
}

void Validator::checkBlockAttrs(const Node* n)
{
    if( !n->meta )
        return;

    const QMap<QString, QString>& a = n->meta->attrs;
    for( QMap<QString, QString>::ConstIterator it = a.constBegin(); it != a.constEnd(); ++it) {
        const QString& key = it.key();

        if( key.startsWith("positional") )
            continue;

        // known valid general attributes
        if( key == "id" || key == "role" || key == "options")
            continue;

        // known valid named attributes for delimited/code blocks
        if( n->kind == Node::K_DelimitedBlock &&
            (key == "source" || key == "language" || key == "linenums"))
            continue;

        if( key == "cols") {
            error(n->pos.row, "'cols' attribute is only valid on table blocks");
            continue;
        }

        // presentation-only attributes not supported in LeanDoc
        if( key == "width" || key == "height") {
            error(n->pos.row, "unsupported presentation attribute '" + key + "'");
            continue;
        }

        warn(n->pos.row, "unknown attribute '" + key + "' on " +
             QString(n->nodeKindName()) + " block");
    }
}

void Validator::checkSourceAttrContext(const Node* n)
{
    if( !n->meta)
        return;

    // [source,lang] should only appear before listing (----) or open (--) blocks
    const QMap<QString, QString>& a = n->meta->attrs;
    bool hasSource = false;
    for( QMap<QString, QString>::ConstIterator it = a.constBegin(); it != a.constEnd(); ++it) {
        if( it.key().startsWith("positional") && it.value() == "source") {
            hasSource = true;
            break;
        }
        if( it.key() == "source") {
            hasSource = true;
            break;
        }
    }
    if( !hasSource)
        return;

    if( n->kind != Node::K_DelimitedBlock)
        return;

    // only listing (----) and open (--) blocks are valid targets
    if( n->delimKind != Node::DK_Listing && n->delimKind != Node::DK_Open)
        error(n->pos.row, "[source] attribute is only valid on listing (----) "
              "or open (--) blocks");
}

void Validator::checkAdmonitionAttrContext(const Node* n)
{
    if( !n->meta)
        return;

    // admonition-type attributes ([NOTE], [TIP], etc.) before non-delimited blocks
    const QMap<QString, QString>& a = n->meta->attrs;
    static const char* admonTypes[] = {"NOTE", "TIP", "WARNING", "CAUTION", "IMPORTANT"};
    const int nadmon = 5;

    for( QMap<QString, QString>::ConstIterator it = a.constBegin(); it != a.constEnd(); ++it) {
        if( !it.key().startsWith("positional"))
            continue;
        const QString v = it.value();
        for( int i = 0; i < nadmon; ++i) {
            if( v == admonTypes[i]) {
                // admonition as block attribute is valid only on open (--) or example (====) blocks
                if( n->kind != Node::K_DelimitedBlock ||
                    (n->delimKind != Node::DK_Open && n->delimKind != Node::DK_Example))
                    error(n->pos.row, "[" + v + "] attribute is only valid on "
                          "open (--) or example (====) delimited blocks");
                break;
            }
        }
    }
}

void Validator::checkRoleContext(const Node* n)
{
    if( !n->meta)
        return;

    // validate roles for correct context
    const QMap<QString, QString>& a = n->meta->attrs;
    for( QMap<QString, QString>::ConstIterator it = a.constBegin(); it != a.constEnd(); ++it) {
        if( it.key() == "role" || (it.key().startsWith("positional") && it.value().startsWith('.'))) {
            QString role = it.value();
            if( role.startsWith('.'))
                role = role.mid(1);

            // bibliography/appendix/glossary/index roles only valid on sections
            if( role == "bibliography" || role == "appendix" ||
                role == "glossary" || role == "index" || role == "abstract") {
                if( n->kind != Node::K_Section)
                    error(n->pos.row, "role '" + role + "' is only valid on section blocks");
            }
        }
    }
}

void Validator::checkColsOnNonTable(const Node* n)
{
    if( !n->meta)
        return;
    // warn about cols attribute on non-table blocks
    if( n->meta->attrs.contains("cols"))
        error(n->pos.row, "'cols' attribute is only valid on table blocks");
}

void Validator::checkXref(const Node* n)
{
    if( n->target.isEmpty()) return;

    // block IDs are case-sensitive and must be explicitly declared
    const QString id = n->target.trimmed();
    if( !danchors.contains(id) )
        warn(n->pos.row, "unresolved cross-reference '<<" + id + ">>'");
}

void Validator::warn(int line, const QString& msg)
{
    diagnostics.append(Diagnostic(Diagnostic::Warning, line, msg));
}

void Validator::error(int line, const QString& msg)
{
    diagnostics.append(Diagnostic(Diagnostic::Error, line, msg));
}
