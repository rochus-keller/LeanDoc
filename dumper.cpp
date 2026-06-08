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

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtDebug>

#include "LeanDocLexer2.h"
#include "LeanDocParser2.h"
#include "LeanDocAst2.h"
#include "LeanDocValidator.h"

using namespace LeanDoc;

static void dumpTokens(const QString& input, QTextStream& out)
{
    Lexer lex;
    lex.setInput(input);

    while (!lex.atEnd()) {
        LineTok t = lex.take();
        out << t.lineNo << ": " << LineTok::kindName(t.kind) << "\n";
    }
    LineTok eof = lex.take();
    out << eof.lineNo << ": " << LineTok::kindName(eof.kind) << "\n";
}

static bool readFileUtf8(const QString& path, QString* outText, QString* outErr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (outErr) *outErr = "Cannot open file: " + path;
        return false;
    }
    QByteArray bytes = f.readAll();
    *outText = QString::fromUtf8(bytes.constData(), bytes.size());
    return true;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        err << "Usage:\n"
            << "  dumper --tokens <file>\n"
            << "  dumper --ast    <file>\n";
        return 2;
    }

    bool modeTokens = false;
    bool modeAst = false;
    QString filePath;

    for (int i=1;i<args.size();++i) {
        if (args[i] == "--tokens")
            modeTokens = true;
        else if (args[i] == "--ast")
            modeAst = true;
        else
            filePath = args[i];
    }

    if (filePath.isEmpty() || (modeTokens == modeAst)) {
        err << "Error: specify exactly one mode and a file.\n";
        return 2;
    }

    QString text, ioErr;
    if (!readFileUtf8(filePath, &text, &ioErr)) {
        err << ioErr << "\n";
        return 2;
    }

    if (modeTokens) {
        dumpTokens(text, out);
        return 0;
    }

    // modeAst
    Parser p;
    Node* doc = p.parse(text);
    if (!doc) {
        err << "Parse failed (null result)\n";
        return 1;
    }

    for (int i = 0; i < p.errors.size(); ++i)
        err << "Error at line " << p.errors[i].pos.row << ": " << p.errors[i].message << "\n";

    // validation pass
    Validator v;
    v.validate(doc);
    for (int i = 0; i < v.diagnostics.size(); ++i) {
        const Diagnostic& d = v.diagnostics[i];
        err << (d.level == Diagnostic::Error ? "Error" : "Warning")
            << " at line " << d.line << ": " << d.message << "\n";
    }

    doc->dump(out);
    Node::deleteTree(doc);

    bool hasErrors = !p.errors.isEmpty();
    for (int i = 0; i < v.diagnostics.size(); ++i) {
        if (v.diagnostics[i].level == Diagnostic::Error)
            hasErrors = true;
    }
    return hasErrors ? 1 : 0;
}
