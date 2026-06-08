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
#include <QtCore/QRegExp>
using namespace LeanDoc;

void Validator::validate(const Node* doc)
{
    diagnostics.clear();
    danchors.clear();

    if( !doc || doc->kind != Node::K_Document)
        return;

    // pass 1: collect all declared anchors (explicit [[id]] and section-derived)
    collectAnchors(doc);

    // pass 2: check attributes, references, etc.
    checkNode(doc);
}

static QString sectionToId(const QString& title)
{
    // derive anchor ID from section title: lowercase, spaces to hyphens,
    // remove non-alnum/non-hyphen/non-underscore
    QString id;
    for( int i = 0; i < title.size(); ++i) {
        const QChar c = title[i];
        if( c.isLetterOrNumber() || c == '_')
            id.append(c.toLower());
        else if( c == ' ' || c == '-')
            id.append('-');
    }
    // collapse consecutive hyphens
    while( id.contains("--"))
        id.replace("--", "-");

    // trim leading/trailing hyphens
    while( id.startsWith('-'))
        id = id.mid(1);
    while( id.endsWith('-'))
        id.chop(1);

    return id;
}

void Validator::collectAnchors(const Node* n)
{
    if( !n )
        return;

    // explicit block anchor from metadata
    if( n->meta && !n->meta->anchorId.isEmpty())
        danchors.insert(n->meta->anchorId);

    // section title -> derived anchor
    if( n->kind == Node::K_Section && !n->name.isEmpty())
        danchors.insert(sectionToId(n->name));

    // inline anchor (stored in name field)
    if( n->kind == Node::K_AnchorInline && !n->name.isEmpty())
        danchors.insert(n->name);

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
        break;
    case Node::K_Xref:
        checkXref(n);
        break;
    case Node::K_DelimitedBlock:
    case Node::K_List:
    case Node::K_Paragraph:
    case Node::K_Section:
        checkBlockAttrs(n);
        break;
    default:
        break;
    }

    for( int i = 0; i < n->children.size(); ++i )
        checkNode(n->children[i]);
}

static bool isValidColSpec(const QString& raw)
{
    // valid formats:
    //   "1,2,3"     - relative widths (positive integers)
    //   "<,^,>"     - alignment chars
    //   "1<,2^,3>"  - combined width+alignment
    // invalid: percentages like "20%,16%,..."
    const QString s = raw.trimmed();
    // strip surrounding quotes if present
    QString inner = s;
    if( inner.startsWith('"') && inner.endsWith('"'))
        inner = inner.mid(1, inner.size()-2);
    if( inner.isEmpty())
        return false;

    const QStringList parts = inner.split(',');
    for( int i = 0; i < parts.size(); ++i) {
        const QString p = parts[i].trimmed();

        if( p.isEmpty() )
            continue;

        // reject percentage notation
        if( p.contains('%') )
            return false;

        // each part must be: [digits] [<^>]
        bool hasDigit = false;
        bool hasAlign = false;
        for( int j = 0; j < p.size(); ++j) {
            const QChar c = p[j];
            if( c.isDigit()) {
                if( hasAlign )
                    return false; // digit after alignment char
                hasDigit = true;
            } else if( c == '<' || c == '^' || c == '>') {
                hasAlign = true;
            } else {
                return false; // unexpected character
            }
        }
        if( !hasDigit && !hasAlign )
            return false;
    }
    return true;
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
            if( inner != "header")
                warn(n->pos.row, "unknown table option '" + inner + "'");
        } else if( key.startsWith("positional")) {
            // positional args: first is typically a block role; ignore for tables
        } else {
            warn(n->pos.row, "unknown table attribute '" + key + "'");
        }
    }
}

void Validator::checkBlockAttrs(const Node* n)
{
    if( !n->meta )
        return;

    const QMap<QString, QString>& a = n->meta->attrs;
    for( QMap<QString, QString>::ConstIterator it = a.constBegin(); it != a.constEnd(); ++it) {
        const QString& key = it.key();

        // known valid positional attributes
        if( key.startsWith("positional") )
            continue;

        // known valid named attributes for delimited/code blocks
        if( key == "source" || key == "language" || key == "linenums" )
            continue;

        // known valid general attributes
        if( key == "id" || key == "role" || key == "options" || key == "cols" )
            continue;

        // presentation-only attributes not supported in LeanDoc
        if( key == "width" || key == "height") {
            warn(n->pos.row, "unsupported presentation attribute '" + key + "'");
        }
    }
}

void Validator::checkXref(const Node* n)
{
    if( n->target.isEmpty()) return;

    // the target might be "id" or "id, display text" — we only care about the id part
    const QString id = n->target.trimmed();

    // try exact match first, then section-derived normalization
    if( danchors.contains(id) )
        return;
    if( danchors.contains(sectionToId(id)) )
        return;

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

