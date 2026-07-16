#include "maploader.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>

QMap<QString, QVariant> MapLoader::parseYaml(const QString& yamlPath)
{
    QMap<QString, QVariant> metaData;

    QFile file(yamlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[MapLoader] 无法打开yaml文件:" << yamlPath;
        return metaData;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        int colonPos = line.indexOf(':');
        if (colonPos == -1) {
            continue;
        }

        QString key = line.left(colonPos).trimmed();
        QString valueStr = line.mid(colonPos + 1).trimmed();

        if (valueStr.startsWith("[")) {
            metaData[key] = parseYamlList(valueStr);
        } else {
            metaData[key] = parseYamlValue(valueStr);
        }
    }

    file.close();
    return metaData;
}

QVariant MapLoader::parseYamlValue(const QString& line)
{
    if (line.isEmpty()) {
        return QString();
    }

    if (line.startsWith("\"") || line.startsWith("'")) {
        return line.mid(1, line.length() - 2);
    }

    bool ok;
    double doubleValue = line.toDouble(&ok);
    if (ok) {
        return doubleValue;
    }

    return line;
}

QList<QVariant> MapLoader::parseYamlList(const QString& line)
{
    QList<QVariant> list;

    QString content = line.mid(1, line.length() - 2).trimmed();
    if (content.isEmpty()) {
        return list;
    }

    QStringList items = content.split(',', Qt::SkipEmptyParts);
    for (const QString& item : items) {
        list.append(parseYamlValue(item.trimmed()));
    }

    return list;
}

QImage MapLoader::loadPgm(const QString& pgmPath)
{
    QFile file(pgmPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[MapLoader] 无法打开pgm文件:" << pgmPath;
        return QImage();
    }

    QTextStream in(&file);
    QString magicNumber = in.readLine().trimmed();

    if (magicNumber != "P2" && magicNumber != "P5") {
        qWarning() << "[MapLoader] 不支持的pgm格式:" << magicNumber;
        file.close();
        return QImage();
    }

    QString line;
    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith("#")) {
            break;
        }
    }

    QStringList sizeParts = line.split(' ', Qt::SkipEmptyParts);
    if (sizeParts.size() < 2) {
        qWarning() << "[MapLoader] 无效的pgm尺寸信息";
        file.close();
        return QImage();
    }

    int width = sizeParts[0].toInt();
    int height = sizeParts[1].toInt();

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith("#")) {
            break;
        }
    }

    int maxGray = line.toInt();

    QImage image(width, height, QImage::Format_Grayscale8);

    if (magicNumber == "P2") {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                while (in.atEnd() || line.isEmpty() || line.startsWith("#")) {
                    line = in.readLine().trimmed();
                }
                int value = line.toInt();
                line.clear();

                int gray = (value * 255) / maxGray;
                image.setPixel(x, y, qRgb(gray, gray, gray));
            }
        }
    } else if (magicNumber == "P5") {
        file.close();
        QFile binaryFile(pgmPath);
        if (!binaryFile.open(QIODevice::ReadOnly)) {
            qWarning() << "[MapLoader] 无法打开pgm二进制文件:" << pgmPath;
            return QImage();
        }

        QByteArray data = binaryFile.readAll();
        int dataOffset = 0;

        while (dataOffset < data.size()) {
            char c = data[dataOffset];
            if (c == '\n') {
                dataOffset++;
                break;
            }
            dataOffset++;
        }

        while (dataOffset < data.size()) {
            char c = data[dataOffset];
            if (c == '\n') {
                dataOffset++;
                break;
            }
            dataOffset++;
        }

        while (dataOffset < data.size()) {
            char c = data[dataOffset];
            if (c == '\n') {
                dataOffset++;
                break;
            }
            dataOffset++;
        }

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (dataOffset >= data.size()) {
                    break;
                }
                unsigned char value = static_cast<unsigned char>(data[dataOffset]);
                dataOffset++;

                int gray = (value * 255) / maxGray;
                image.setPixel(x, y, qRgb(gray, gray, gray));
            }
        }

        binaryFile.close();
    } else {
        file.close();
        return QImage();
    }

    file.close();
    return image;
}

nav_msgs::msg::OccupancyGrid::SharedPtr MapLoader::loadFromFile(const QString& yamlPath)
{
    QMap<QString, QVariant> metaData = parseYaml(yamlPath);

    if (!metaData.contains("image") || !metaData.contains("resolution") || !metaData.contains("origin")) {
        qWarning() << "[MapLoader] yaml文件缺少必要的元数据";
        return nullptr;
    }

    QString pgmPath = QFileInfo(yamlPath).absolutePath() + "/" + metaData["image"].toString();
    QImage image = loadPgm(pgmPath);

    if (image.isNull()) {
        qWarning() << "[MapLoader] 无法加载pgm图像:" << pgmPath;
        return nullptr;
    }

    auto map = std::make_shared<nav_msgs::msg::OccupancyGrid>();

    map->info.width = image.width();
    map->info.height = image.height();
    map->info.resolution = metaData["resolution"].toDouble();

    QList<QVariant> originList = metaData["origin"].toList();
    if (originList.size() >= 3) {
        map->info.origin.position.x = originList[0].toDouble();
        map->info.origin.position.y = originList[1].toDouble();
        map->info.origin.position.z = originList[2].toDouble();
    } else {
        map->info.origin.position.x = 0.0;
        map->info.origin.position.y = 0.0;
        map->info.origin.position.z = 0.0;
    }

    map->info.origin.orientation.x = 0.0;
    map->info.origin.orientation.y = 0.0;
    map->info.origin.orientation.z = 0.0;
    map->info.origin.orientation.w = 1.0;

    map->data.resize(image.width() * image.height());

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QRgb pixel = image.pixel(x, y);
            int gray = qGray(pixel);
            int value = (255 - gray) * 100 / 255;
            map->data[y * image.width() + x] = static_cast<int8_t>(value);
        }
    }

    return map;
}
