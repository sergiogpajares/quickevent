#include "application.h"

#include <qf/core/log.h>
#include <qf/core/assert.h>
#include <qf/core/utils/fileutils.h>

#include <QQmlEngine>
#include <QQmlContext>
#include <QFile>
#include <QJsonParseError>

namespace qfu = qf::core::utils;
using namespace qf::gui::framework;

Application::Application(int &argc, char **argv) :
	Super(argc, argv)
{
}

QString Application::versionString() const
{
	return QCoreApplication::applicationVersion();
}

Application::~Application()
= default;

Application *Application::instance(bool must_exist)
{
	auto *ret = qobject_cast<Application*>(Super::instance());
	if(!ret && must_exist) {
		qfFatal("qf::gui::framework::Application instance MUST exist.");
	}
	return ret;
}

MainWindow *Application::frameWork()
{
	QF_ASSERT_EX(m_frameWork != nullptr, "FrameWork is not set.");
	return m_frameWork;
}

QString Application::applicationDirPath()
{
	return QCoreApplication::applicationDirPath();
}

QString Application::applicationName()
{
	return QCoreApplication::applicationName();
}

QStringList Application::arguments()
{
	return QCoreApplication::arguments();
}

void Application::loadStyleSheet(const QString &file)
{
	QString css_file_name = file;
	if(css_file_name.isEmpty()) {
		QString app_name = Application::applicationName().toLower();
		css_file_name = qfu::FileUtils::joinPath(Application::applicationDirPath(), "/" + app_name + "-data/style/default.css");
		if(!QFile::exists(css_file_name))
			css_file_name = ":/" + app_name + "/style/default.css";
	}
	qfInfo() << "Opening style sheet:" << css_file_name;
	QFile f(css_file_name);
	if(f.open(QFile::ReadOnly)) {
		QByteArray ba = f.readAll();
		QString ss = QString::fromUtf8(ba);
		setStyleSheet(ss);
	}
	else {
		qfWarning() << "Cannot open style sheet:" << css_file_name;
	}
}

