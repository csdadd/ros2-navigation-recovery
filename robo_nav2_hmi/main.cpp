#include "mainwindow.h"
#include "loglevel.h"
#include <QMetaType>

#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <QCommandLineParser>
#include <QScreen>

/**
 * @brief 应用程序入口
 * @details 使用两阶段初始化模式，确保资源正确释放
 */
int main(int argc, char *argv[])
{
    try {
        qRegisterMetaType<LogLevel>("LogLevel");
        QApplication a(argc, argv);

        // 命令行参数解析
        QCommandLineParser parser;
        parser.setApplicationDescription("WheelTec Robot Control Interface");
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption verboseOption({"v", "verbose"}, "启用详细日志输出");
        QCommandLineOption logLevelOption("log-level", "设置日志级别 (debug/info/warn/error)", "level", "info");

        parser.addOption(verboseOption);
        parser.addOption(logLevelOption);
        parser.process(a);

        // 启用统一的日志输出格式，确保 Release 模式下也能看到调试信息
        qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] %{message}");

        if (parser.isSet(verboseOption)) {
            qDebug() << "[Main] 详细日志模式已启用";
        }

        QString logLevel = parser.value(logLevelOption);
        if (!logLevel.isEmpty()) {
            qDebug() << "[Main] 日志级别设置为:" << logLevel;
        }

        qDebug() << "[Main] 正在创建主界面...";

        MainWindow w;

        // 两阶段初始化，避免构造函数中调用exit
        if (!w.initialize()) {
            qCritical() << "[Main] 错误：主界面初始化失败！";
            return -1;
        }

        qDebug() << "[Main] 主界面创建成功，正在显示...";

        w.show();
        w.activateWindow();
        w.raise();

        // 强制窗口居中显示
        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            int x = (screenGeometry.width() - w.width()) / 2;
            int y = (screenGeometry.height() - w.height()) / 2;
            w.move(x, y);
        }

        if (!w.isVisible()) {
            qCritical() << "[Main] 错误：主界面无法显示！";
            QMessageBox::critical(nullptr, "错误", "主界面无法显示！\n\n程序可能无法正常运行。");
            return -1;
        }

        qDebug() << "[Main] 主界面已成功显示，进入事件循环...";

        return a.exec();//进入 Qt 事件循环，阻塞，直到窗口关闭
    } catch (const std::exception& e) {//将e绑定到被上面抛出的类型兼容的对象
        qCritical() << "[Main] 发生异常:" << e.what();
        QMessageBox::critical(nullptr, "错误", QString("程序发生异常:\n%1").arg(e.what()));
        return -1;
    } catch (...) {
        qCritical() << "[Main] 发生未知异常";
        QMessageBox::critical(nullptr, "错误", "程序发生未知异常！");
        return -1;
    }
}
