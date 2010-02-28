#include <cstdio>
#include <QtCore>
#include <QtScript>

const int WRONG_USAGE = 1;
const int MISSING_FILE = 2;
const int PARSE_ERROR = 3;
const int LINT_ERROR = 4;

void out(const QString& message) { std::fprintf(stdout, "%s\n", qPrintable(message)); }
void err(const QString& message) { std::fprintf(stderr, "%s\n", qPrintable(message)); }

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	if (app.arguments().count() < 2) {
		err("Usage: qtslint file1.js (file2.js ...)");
		return WRONG_USAGE;
	}
	QStringList inputFileNames = app.arguments();
	inputFileNames.removeAt(0);

	QScriptEngine engine;

	// load jslint
	QFile jslintFile("fulljslint.js");
	jslintFile.open(QIODevice::ReadOnly | QIODevice::Text);
	if (!jslintFile.isOpen()) {
		err("Can't locate " + jslintFile.fileName());
		return MISSING_FILE;
	}

	Q_ASSERT(jslintFile.isOpen());
	QTextStream jslintStream(&jslintFile);
	engine.evaluate(jslintStream.readAll());

	// test input files
	foreach(const QString& inputFileName, inputFileNames) {
		// open file
		QFile inputFile(inputFileName);
		inputFile.open(QIODevice::ReadOnly | QIODevice::Text);
		if (!inputFile.isOpen()) {
			err("Can't locate " + inputFileName);
			return MISSING_FILE;
		}

		// load text into array
		err("Checking " + inputFileName);
		QString inputData = QTextStream(&inputFile).readAll();
		inputData.replace("'", "\\'");
		inputData = "[ '" + inputData.replace("\n", "', '") + "' ]";

		// run jslint
		const QString cmd = QString("JSLINT(%1)").arg(inputData);
		const QScriptValue res = engine.evaluate(cmd);
		if (engine.hasUncaughtException()) {
			err("Uncaught exception at parsing " + inputFileName);
			err("\t" + engine.uncaughtException().toString());
			const QStringList backtrace = engine.uncaughtExceptionBacktrace();
			foreach(const QString& line, backtrace) {
				err("\t\t" + line);
			}
			return PARSE_ERROR;
		}

		// check exit status
		if (!res.toBool()) {
			// print errors to console
			QScriptValueIterator it(engine.evaluate("JSLINT.errors"));
			while (it.hasNext()) {
				it.next();
				const QScriptValue error = it.value();
				const int line = error.property("line").toInt32();
				const int character = error.property("character").toInt32();
				const QString reason = error.property("reason").toString();
				const QString evidence = error.property("evidence").toString();

				out(QString("%1) Problem at line %2 character %3: '%4'").arg(it.name().toInt() + 1).arg(line).arg(character).arg(evidence));
				out("\t" + reason);
				static QStringList detailsProps(QString("a b c d").split(" "));
				foreach(const QString& detailsProp, detailsProps) {
					const QScriptValue detail = error.property(detailsProp);
					if (!detail.isUndefined()) {
						out("\t" + detail.toString());
					}
				}
			}

			// generate report
			const QScriptValue report = engine.evaluate("JSLINT.report(false)");
			const QString html = "<html><body>\n" + report.toString() + "\n</body></html>\n";
			QFile reportFile("report.html");
			reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
			reportFile.write(html.toUtf8());
			err("Report for " + inputFileName + " generated.");
			return LINT_ERROR;
		}

		err(inputFileName + " looks good.");
	}

	return 0;
}
