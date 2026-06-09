#ifndef LEANDOC_VALIDATOR_H
#define LEANDOC_VALIDATOR_H

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
#include <QtCore/QSet>
#include "LeanDocAst2.h"

namespace LeanDoc {

struct Diagnostic {
    enum Level { Warning, Error };
    Level level;
    int line;
    QString message;
    Diagnostic():level(Warning),line(0){}
    Diagnostic(Level lv, int l, const QString& m):level(lv),line(l),message(m){}
};

class Validator {
public:
    void validate(const Node* doc);
    QList<Diagnostic> diagnostics;

private:
    void collectAnchors(const Node* n);
    void checkNode(const Node* n);

    void checkTableAttrs(const Node* n);
    void checkBlockAttrs(const Node* n);
    void checkXref(const Node* n);
    void checkSourceAttrContext(const Node* n);
    void checkRoleContext(const Node* n);
    void checkColsOnNonTable(const Node* n);
    void checkTableCellCount(const Node* n);
    void checkAdmonitionAttrContext(const Node* n);

    void warn(int line, const QString& msg);
    void error(int line, const QString& msg);

    QSet<QString> danchors; // declared anchor IDs
    QMap<QString, int> danchorLines; // anchor ID -> first occurrence line
};

} // namespace LeanDoc

#endif
