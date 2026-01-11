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

#include "LeanDocLexer2.h"
#include "LeanDocParser2.h"
#include "LeanDocTypstGen.h"
#include "LeanDocAst2.h"

using namespace LeanDoc;

static bool readFileUtf8(const QString& path, QString* outText, QString* outErr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (outErr)
            *outErr = "Cannot open file: " + path;
        return false;
    }
    QByteArray bytes = f.readAll();
    *outText = QString::fromUtf8(bytes.constData(), bytes.size());
    return true;
}

static bool writeFileUtf8(const QString& path, const QString& text, QString* outErr)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outErr)
            *outErr = "Cannot write file: " + path;
        return false;
    }
    f.write(text.toUtf8());
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
            << "  leandoc2typst --typst  <in.adoc> -o <out.typ> [--template plain|report] [--template-file tpl.typ]\n";
        return 2;
    }

    bool modeAst = false, modeTypst = true;
    QString inPath, outPath = "output.typ";
    TypstGenerator::Options genOpt;

    for (int i=1;i<args.size();++i) {
        const QString a = args[i];
        if (a == "--ast") {
            modeAst = true;
            modeTypst = false;
        }else if (a == "-o" && i+1 < args.size())
            outPath = args[++i];
        else if (a == "--template" && i+1 < args.size())
            genOpt.templateName = args[++i];
        else if (a == "--template-file" && i+1 < args.size())
            genOpt.templateFile = args[++i];
        else if (a == "--no-raw")
            genOpt.allowRawPassthrough = false;
        else if (!a.startsWith("-") && inPath.isEmpty())
            inPath = a;
    }

    if (inPath.isEmpty() || (modeAst + modeTypst) != 1) {
        err << "Error: choose exactly one mode and provide an input file.\n";
        return 2;
    }
    if (modeTypst && outPath.isEmpty()) {
        err << "Error: --typst requires -o <out.typ>\n";
        return 2;
    }

    QString text, ioErr;
    if (!readFileUtf8(inPath, &text, &ioErr)) {
        err << ioErr << "\n";
        return 2;
    }

    Parser parser;
    ParseError pe;
    Node* doc = parser.parse(text, &pe);
    if (!doc) {
        err << "Parse error at line " << pe.line << ": " << pe.message << "\n";
        return 1;
    }

    if( modeAst )
    {
        QTextStream out(stdout);
        doc->dump(out,0);
        return 0;
    }

    // modeTypst
    TypstGenerator gen(genOpt);
    TypstGenError ge;
    QString typ;
    QTextStream typOut(&typ);

    if (!gen.generate(doc, typOut, &ge)) {
        err << "Typst generation error at line " << ge.line << ": " << ge.message << "\n";
        Node::deleteTree(doc);
        return 1;
    }

    Node::deleteTree(doc);

    if (!writeFileUtf8(outPath, typ, &ioErr)) {
        err << ioErr << "\n";
        return 2;
    }

    out << "Wrote " << outPath << "\n";
    return 0;
}

