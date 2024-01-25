/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "VipMapFileSystem.h"
#include "VipArchive.h"
#include "VipEnvironment.h"
#include "VipLogging.h"
#include "VipProgress.h"
#include "VipSleep.h"

#include <QMutex>
#include <QRegExp>

VipPath::VipPath()
  : m_dir(false)
{
}
VipPath::VipPath(const VipPath& other)
  : m_map(other.m_map)
  , m_attributes(other.attributes())
  , m_canonical_path(other.canonicalPath())
  , m_dir(other.isDir())
{
}

VipPath::VipPath(const QString& full_path, bool is_dir)
  : m_canonical_path(full_path)
  , m_dir(is_dir)
{
	// replace backslash by slash
	m_canonical_path.replace("\\", "/");
	// remove starting and trailing slash
	if (m_canonical_path.endsWith("/") && m_canonical_path.size() != 1)
		m_canonical_path = m_canonical_path.mid(0, m_canonical_path.size() - 1);
}

VipPath::VipPath(const QString& full_path, const QVariantMap& attributes, bool is_dir)
  : m_attributes(attributes)
  , m_canonical_path(full_path)
  , m_dir(is_dir)
{
	// replace backslash by slash
	m_canonical_path.replace("\\", "/");
	// remove starting and trailing slash
	if (m_canonical_path.endsWith("/") && m_canonical_path.size() != 1)
		m_canonical_path = m_canonical_path.mid(0, m_canonical_path.size() - 1);
}

VipPath& VipPath::operator=(const VipPath& other)
{
	m_canonical_path = other.canonicalPath();
	m_attributes = other.attributes();
	m_dir = other.isDir();
	m_map = other.m_map;
	return *this;
}

bool VipPath::isEmpty() const
{
	return m_canonical_path.isEmpty();
}

QString VipPath::lastPath() const
{
	int index = m_canonical_path.lastIndexOf("/");
	if (index >= 0)
		return m_canonical_path.mid(index + 1);
	else
		return QString();
}

QString VipPath::fileName() const
{
	if (!m_dir)
		return lastPath();
	else
		return QString();
}

QString VipPath::filePath() const
{
	if (m_dir)
		return m_canonical_path;
	else {
		int index = m_canonical_path.lastIndexOf("/");
		if (index >= 0)
			return m_canonical_path.mid(0, index);
		else
			return QString();
	}
}

VipPath VipPath::parent() const
{
	QStringList lst = m_canonical_path.split("/", VIP_SKIP_BEHAVIOR::KeepEmptyParts);
	if (lst.size() > 1) {
		lst.removeAt(lst.size() - 1);
		return VipPath(lst.join("/"), true);
	}
	return VipPath();
}

void VipPath::setMapFileSystem(const VipMapFileSystemPtr& map)
{
	m_map = map;
}

VipMapFileSystemPtr VipPath::mapFileSystem() const
{
	return m_map;
}

QStringList VipPath::mergeAttributes(const QVariantMap& attrs)
{
	QStringList res;
	for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
		QVariantMap::const_iterator found = m_attributes.find(it.key());
		if (found == m_attributes.end() || it.value() != found.value()) {
			m_attributes[it.key()] = it.value();
			res << it.key();
		}
	}
	return res;
}

bool operator==(const VipPath& p1, const VipPath& p2)
{
	return p1.canonicalPath() == p2.canonicalPath();
}

bool operator!=(const VipPath& p1, const VipPath& p2)
{
	return p1.canonicalPath() != p2.canonicalPath();
}

static VipArchive& operator<<(VipArchive& arch, const VipPath& p)
{
	arch.content("path", p.canonicalPath());
	arch.content("is_dir", p.isDir());
	return arch;
}

static VipArchive& operator>>(VipArchive& arch, VipPath& p)
{
	QString path = arch.read("path").toString();
	bool is_dir = arch.read("is_dir").toBool();
	p = VipPath(path, is_dir);
	return arch;
}

static VipArchive& operator<<(VipArchive& arch, const VipPathList& p)
{
	arch.content("count", p.count());
	arch.start("paths");

	for (int i = 0; i < p.size(); ++i)
		arch.content(p[i]);
	arch.end();

	return arch;
}

static VipArchive& operator>>(VipArchive& arch, VipPathList& p)
{
	int count = arch.read("count").toInt();
	arch.start("paths");
	p.clear();
	for (int i = 0; i < count; ++i) {
		p.append(arch.read().value<VipPath>());
	}
	arch.end();
	return arch;
}

static int r1 = qRegisterMetaType<VipPath>("VipPath");
static int r2 = qRegisterMetaType<VipPathList>("VipPathList");

static int registerPath()
{
	vipRegisterArchiveStreamOperators<VipPath>();
	vipRegisterArchiveStreamOperators<VipPathList>();
	return 0;
}
static int _registerPath = vipAddInitializationFunction(registerPath);

#include <qthread.h>

class SearchThread : public QThread
{
public:
	VipMapFileSystem* map;
	VipPath path;
	QList<QRegExp> exps;
	QDir::Filters filters;
	bool exact;
	bool stop = false;
	int count_found;

	SearchThread(VipMapFileSystem* map, const VipPath& path, const QList<QRegExp>& exps, QDir::Filters filters, bool exact)
	  : map(map)
	  , path(path)
	  , exps(exps)
	  , filters(filters)
	  , exact(exact)
	  , count_found(0)
	{
		start();
	}

	~SearchThread()
	{
		if (isRunning())
			stop = true;
		wait();
	}

	bool match(const VipPath& p)
	{
		QString _path = p.canonicalPath();
		if (_path.isEmpty())
			return false;

		// find the last part of the pasth, after the last '/'
		int index = _path.lastIndexOf("/");
		if (index == _path.size() - 1)
			index = _path.lastIndexOf("/", _path.size() - 2);
		if (index >= 0)
			_path = _path.mid(index + 1);

		for (int i = 0; i < exps.size(); ++i) {
			if (exact) {
				if (exps[i].exactMatch(_path))
					return true;
			}
			else {
				int id = exps[i].indexIn(_path);
				if (id == 0)
					return true;
			}
		}
		return false;
	}

	void searchHelper(const VipPath& p)
	{
		if (!stop) {
			if (match(p)) {
				Q_EMIT map->found(p);
				map->m_found.append(p);

				++count_found;
				if (count_found % 10 == 0)
					vipProcessEvents(nullptr, 10);
			}

			if (p.isDir()) {
				VipPathList lst = map->list(p);
				if (map->hasError())
					return;

				Q_EMIT map->searchEnterPath(p);
				for (int i = 0; i < lst.size(); ++i)
					searchHelper(lst[i]);
			}
		}
	}

	virtual void run()
	{
		Q_EMIT map->searchStarted();
		// searchHelper(path);

		VipPathList lst;
		lst.append(path);

		while (lst.size() && !stop && exps.size()) {
			// organize the search by layers
			VipPathList next_layer;

			for (int i = 0; i < lst.size(); ++i) {
				if (stop)
					break;

				Q_EMIT map->searchEnterPath(lst[i]);

				// for all path of the current layer, list
				VipPathList tmp = map->list(lst[i]);
				if (map->hasError()) {
					stop = true;
					break;
				}

				// inspect all found elements
				for (int j = 0; j < tmp.size(); ++j) {
					if (match(tmp[j])) {
						Q_EMIT map->found(tmp[j]);
						map->m_found.append(tmp[j]);

						++count_found;
						if (count_found % 10 == 0)
							vipProcessEvents(nullptr, 10);
					}

					if (tmp[j].isDir())
						next_layer.append(tmp[j]);
				}
			}

			lst = next_layer;
		}

		Q_EMIT map->searchEnded();
	}
};

VipMapFileSystem::VipMapFileSystem(SupportedOperations operations)
  : m_error_code(0)
  , m_operations(operations)
  , m_search(nullptr)
{
	VipUniqueId::id(this);
}

VipMapFileSystem::~VipMapFileSystem()
{
	if (m_search) {
		delete m_search;
	}
}

QSharedPointer<VipMapFileSystem> VipMapFileSystem::sharedPointer() const
{
	return const_cast<VipMapFileSystem*>(this)->sharedFromThis();
}

VipLazyPointer VipMapFileSystem::lazyPointer() const
{
	return VipLazyPointer(const_cast<VipMapFileSystem*>(this));
}

void VipMapFileSystem::openIfNecessary()
{
	if (!isOpen())
		open(address());
}

VipPathList VipMapFileSystem::roots()
{
	resetError();
	return rootPaths();
}

QStringList VipMapFileSystem::standardAttributes()
{
	resetError();
	return standardFileSystemAttributes();
}

bool VipMapFileSystem::exists(const VipPath& path)
{
	resetError();
	return pathExists(path);
}

bool VipMapFileSystem::create(const VipPath& path)
{
	if (!(m_operations & Create)) {
		setError("Cannot create path: unsupported operation", -1);
		return false;
	}
	resetError();

	QStringList paths = path.canonicalPath().split("/", VIP_SKIP_BEHAVIOR::KeepEmptyParts);
	QString subpath;
	for (int i = 0; i < paths.size() - 1; ++i) {
		if (i == 0) {
			if (paths[i].isEmpty())
				subpath = "/";
			else
				subpath = paths[0];
		}
		else
			subpath += "/" + paths[i];

		VipPath p(subpath, true);
		if (!exists(p))
			if (!createPath(p))
				return false;
	}

	return createPath(path);
}

bool VipMapFileSystem::remove(const VipPath& path)
{
	if (!(m_operations & Remove)) {
		setError("Cannot remove path: unsupported operation", -1);
		return false;
	}
	resetError();
	vip_debug("%s\n", path.canonicalPath().toLatin1().data());
	return removePath(path);
}

bool VipMapFileSystem::move(const VipPath& src, const VipPath& dst, bool merge, VipProgress* progress)
{
	if (!(m_operations & Rename)) {
		setError("Cannot move path: unsupported operation", -1);
		return false;
	}
	if (dst.isDir() != src.isDir()) {
		setError("Move: unauthorized operation");
		return false;
	}

	resetError();

	if (progress)
		progress->setText("Move to <b>" + dst.canonicalPath() + "</b>");

	if (!src.isDir()) {
		// if src is a file, just rename it
		return rename(src, dst, merge);
	}
	else {
		bool has_dst_dir = exists(dst);
		if (hasError())
			return false;

		if (!has_dst_dir) {
			// no dst folder with this name: just rename it
			return rename(src, dst, false);
		}
		else {
			if (!merge) {
				setError("Move: destination folder already exists");
				return false;
			}

			// merge src and dst folder
			if (copy(src, dst, true, progress)) {
				if (progress)
					progress->setText("Remove <b>" + src.canonicalPath() + "</b>");

				return removePath(src);
			}
			else
				return false;
		}
	}
}

bool VipMapFileSystem::rename(const VipPath& src, const VipPath& dst, bool overwrite)
{
	if (!(m_operations & Rename)) {
		setError("Cannot rename path: unsupported operation", -1);
		return false;
	}
	resetError();

	if (src.isDir() != dst.isDir()) {
		setError("Rename: unauthorized operation");
		return false;
	}

	if (src.mapFileSystem() && src.mapFileSystem().data() != this) {
		// we try to rename a source file/folder from a different VipMapFileSystem: use copy instead
		return copy(src, dst, overwrite);
	}

	// if the path exists and overwrite is true, remove the destination path first
	if (pathExists(dst)) {
		if (overwrite) {
			if (removePath(dst)) {
				return renamePath(src, dst);
			}
			else {
				setError("Rename: cannot overwrite destination");
				return false;
			}
		}
		else {
			setError("Cannot rename file or directory: destination file or directpory already exists");
			return false;
		}
	}
	else
		return renamePath(src, dst);
}

bool VipMapFileSystem::copy(const VipPath& src, const VipPath& dst, bool overwrite, VipProgress* progress)
{
	if (dst.isDir() != src.isDir()) {
		setError("Copy: unauthorized operation");
		return false;
	}
	if (!(m_operations & CopyFile)) {
		setError("Copy: unsupported operation", -1);
		return false;
	}

	resetError();

	if (src.mapFileSystem() && src.mapFileSystem().data() != this) {
		// we try to copy a source file/folder from a different VipMapFileSystem:
		// copy the src in a temporay directory, and then copy the tmp file to the destination

		// copy a file
		if (!src.isDir()) {
			bool dst_exists = exists(dst);
			if (hasError())
				return false;

			if (dst_exists && !overwrite) {
				setError("Copy: destination file already exists");
				return false;
			}
			else if (dst_exists && overwrite) {
				if (progress)
					progress->setText("Remove <b>" + dst.canonicalPath() + "</b>");
				if (!remove(dst) || hasError())
					return false;
			}

			QIODevice* src_device = src.mapFileSystem()->open(src, QIODevice::ReadOnly);
			QIODevice* dst_device = open(dst, QIODevice::WriteOnly);
			if (progress)
				progress->setText("Copy <b>" + dst.canonicalPath() + "</b>");

			// copy device to device by chunks of 100KB
			qint64 full_size = 0;
			while (true) {
				QByteArray ar(100000, 0);
				qint64 size = src_device->read(ar.data(), ar.size());
				dst_device->write(ar.data(), size);
				full_size += size;
				if (ar.size() != size)
					break;
			}
			return full_size == src_device->size();
		}
		else {
			// copy directory
			bool dst_exists = exists(dst);
			if (hasError())
				return false;

			if (!dst_exists)
				if (!create(dst))
					return false;

			return copyDirContentHelper(src, dst, overwrite, progress);
		}
	}

	if (!src.isDir()) {
		// copy a file

		bool dst_exists = exists(dst);
		if (hasError())
			return false;

		if (dst_exists) {
			if (!overwrite) {
				setError("Copy: destination file already exists");
				return false;
			}

			if (progress)
				progress->setText("Remove <b>" + dst.canonicalPath() + "</b>");

			if (!remove(dst) || hasError())
				return false;

			if (progress)
				progress->setText("Copy <b>" + dst.canonicalPath() + "</b>");

			return copyPath(src, dst);
		}
		else {
			if (progress)
				progress->setText("Copy <b>" + dst.canonicalPath() + "</b>");

			return copyPath(src, dst);
		}
	}
	else {
		// copy directory
		bool dst_exists = exists(dst);
		if (hasError())
			return false;

		if (!dst_exists)
			if (!create(dst))
				return false;

		return copyDirContentHelper(src, dst, overwrite, progress);
	}
	VIP_UNREACHABLE();
	// return false;
}

void VipMapFileSystem::listPathHelper(VipPathList& out, const VipPath& path)
{
	VipPathList tmp = listPathContent(path);
	if (hasError())
		return;

	out += tmp;

	for (int i = 0; i < tmp.size(); ++i) {
		if (tmp[i].isDir()) {
			listPathHelper(out, tmp[i]);
			if (hasError())
				break;
		}
	}
}

VipPathList VipMapFileSystem::list(const VipPath& path, bool recursive)
{
	resetError();
	if (!path.isDir()) {
		setError("Cannot list content on a file");
		return VipPathList();
	}

	if (!recursive)
		return listPathContent(path);
	else {
		VipPathList out;
		listPathHelper(out, path);
		return out;
	}
}

bool VipMapFileSystem::copyDirContentHelper(const VipPath& src_dir, const VipPath& dst_dir, bool overwrite, VipProgress* progress)
{
	VipPathList content_src = list(src_dir, true);
	VipPathList src_files;
	for (int i = 0; i < content_src.size(); ++i)
		if (!content_src[i].isDir())
			src_files.append(content_src[i]);

	if (progress)
		progress->setRange(0, src_files.size());

	for (int i = 0; i < src_files.size(); ++i) {
		if (progress) {
			if (progress->canceled())
				break;
			progress->setValue(i);
			progress->setText("Copy <b>" + src_files[i].canonicalPath() + "</b>");
		}

		QString relative_dir = src_files[i].filePath();
		relative_dir.remove(src_dir.canonicalPath());

		VipPath dst(dst_dir.canonicalPath() + relative_dir, true);
		bool dir_exists = exists(dst);
		if (hasError())
			return false;

		if (!dir_exists)
			create(dst);

		VipPath dst_file(dst.canonicalPath() + "/" + src_files[i].fileName());

		if (!copy(src_files[i], dst_file, overwrite))
			return false;
	}
	return true;
}

QIODevice* VipMapFileSystem::open(const VipPath& path, QIODevice::OpenMode mode)
{
	// setError("Cannot list path content: unsupported operation" - 1);
	if ((mode & QIODevice::ReadOnly) && !(m_operations & OpenRead))
		return nullptr;
	if ((mode & QIODevice::WriteOnly) && !(m_operations & OpenWrite))
		return nullptr;
	if ((mode & QIODevice::Text) && !(m_operations & OpenText))
		return nullptr;
	resetError();
	return openPath(path, mode);
}

void VipMapFileSystem::search(const VipPath& in_path, const QList<QRegExp>& exps, bool exact_match, QDir::Filters filters)
{
	stopSearch();
	m_search = new SearchThread(this, in_path, exps, filters, exact_match);
}

void VipMapFileSystem::stopSearch()
{
	if (m_search) {
		delete m_search;
		m_search = nullptr;
	}
}

VipPathList VipMapFileSystem::searchResults() const
{
	return m_found;
}

VipMapFileSystem::SupportedOperations VipMapFileSystem::supportedOperations() const
{
	return m_operations;
}

bool VipMapFileSystem::hasError() const
{
	return m_error_code < 0;
}

QString VipMapFileSystem::errorString() const
{
	return m_error_string;
}

int VipMapFileSystem::errorCode() const
{
	return m_error_code;
}

void VipMapFileSystem::setError(const QString& error_str, int error_code)
{
	m_error_string = error_str;
	m_error_code = error_code;
}

void VipMapFileSystem::resetError()
{
	m_error_string.clear();
	m_error_code = 0;
	openIfNecessary();
}

bool VipMapFileSystem::createPath(const VipPath&)
{
	return false;
}

bool VipMapFileSystem::removePath(const VipPath&)
{
	return false;
}

bool VipMapFileSystem::renamePath(const VipPath&, const VipPath&)
{
	return false;
}

bool VipMapFileSystem::copyPath(const VipPath&, const VipPath&)
{
	return false;
}

QIODevice* VipMapFileSystem::openPath(const VipPath&, QIODevice::OpenMode)
{
	return nullptr;
}

#include <QFile>
#include <QStorageInfo>
#include <qdatetime.h>
#include <qdir.h>

// On Windows, having network mounted drive in unconnected state causes a LOT of problems.
// It freezes calls to QFileInfo::exists and also cripple the icon provider.
// We try to detect these cases to avoid using the QFileIconProvider that periodically freeze the application.
static std::atomic<bool> _has_network_issue{ false };

VipPhysicalFileSystem::VipPhysicalFileSystem()
  : VipMapFileSystem(VipMapFileSystem::All)
{
	setObjectName("Local file system");
}

QStringList VipPhysicalFileSystem::standardFileSystemAttributes()
{
	static QStringList attrs = QStringList() << "Size"
						 << "Type"
						 << "Last modified"
						 << "Created"
						 << "Last read"
						 << "Executable"
						 << "Writable"
						 << "Readable";
	return attrs;
}

bool VipPhysicalFileSystem::has_network_issues()
{
	return _has_network_issue;
}

bool VipPhysicalFileSystem::exists_timeout(const QString& path, int milli_timeout, bool* timed_out)
{
#ifdef WIN32
	if (milli_timeout < 0) {
		return QFileInfo(path).exists();
	}

	std::atomic<bool> finished = false;
	std::atomic<bool> exists = false;
	std::thread* th = new std::thread([&]() {
		QFileInfo info(path);
		exists = info.exists();
		finished = true;
	});

	qint64 st = QDateTime::currentMSecsSinceEpoch();
	while (QDateTime::currentMSecsSinceEpoch() - st < milli_timeout && !finished)
		QThread::msleep(5);
	if (QDateTime::currentMSecsSinceEpoch() - st >= milli_timeout && timed_out) {
		_has_network_issue = true;
		*timed_out = true;
	}
	else if (timed_out)
		*timed_out = false;

	th->detach();
	delete th;

	return exists;
#else
	return QFileInfo(path).exists();
#endif
}

VipPathList VipPhysicalFileSystem::rootPaths()
{
#ifdef WIN32
	VipPathList res;

	for (char c = 'A'; c <= 'Z'; ++c) {
		QString path = QString(1, c) + ":";
		bool time_out;
		if (exists_timeout(path, 500, &time_out))
			res.append(VipPath(path, QVariantMap(), true));
	}
	return res;
#else
	QList<QStorageInfo> lst = QStorageInfo::mountedVolumes();
	VipPathList res;
	for (int i = 0; i < lst.size(); ++i)
		res << VipPath(lst[i].rootPath(), QVariantMap(), true);
	return res;
#endif
}

bool VipPhysicalFileSystem::pathExists(const VipPath& path)
{
	QString p = path.canonicalPath();
	
#ifdef WIN32
	vip_debug("Check path '%s'\n", p.toLatin1().data());
	// Attempt to correct Qt bug when checking network drives
	//if (p.endsWith(":")) {
		vipSleep(50);
	//}
#endif
	bool ret = QFileInfo(p).exists();

#ifdef WIN32
	vip_debug("Done\n");
#endif
	return ret;
}

bool VipPhysicalFileSystem::createPath(const VipPath& path)
{
	if (path.isDir()) {
		return QDir().mkpath(path.canonicalPath());
	}
	else if (!pathExists(path)) {
		QFile file(path.canonicalPath());
		return file.open(QFile::WriteOnly);
	}
	return false;
}

bool VipPhysicalFileSystem::removePath(const VipPath& path)
{
	if (pathExists(path)) {
		if (path.isDir())
			return QDir(path.canonicalPath()).removeRecursively();
		else
			return QFile::remove(path.canonicalPath());
	}
	return false;
}

bool VipPhysicalFileSystem::renamePath(const VipPath& src, const VipPath& dst)
{
	return QDir().rename(src.canonicalPath(), dst.canonicalPath());
}

bool VipPhysicalFileSystem::copyPath(const VipPath& src, const VipPath& dst)
{
	return QFile::copy(src.canonicalPath(), dst.canonicalPath());
}

VipPathList VipPhysicalFileSystem::listPathContent(const VipPath& path)
{
	VipPathList res;
	QFileInfoList lst = QDir(path.canonicalPath() + "/").entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name);
	for (int i = 0; i < lst.size(); ++i) {
		const QFileInfo& info = lst[i];
		VipPath p;

		if (info.isDir()) {
			p = VipPath(info.canonicalFilePath(), QVariantMap(), true);
			p.setAttribute("Type", "DIR");
			p.setAttribute("Writable", info.isWritable());
			p.setAttribute("Readable", info.isReadable());
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
			p.setAttribute("Created", info.birthTime());
#else
			p.setAttribute("Created", info.created());
#endif
			p.setAttribute("Last modified", info.lastModified());
			p.setAttribute("Last read", info.lastRead());
		}
		else {
			p = VipPath(info.canonicalFilePath(), QVariantMap(), false);
			p.setAttribute("Size", info.size());
			p.setAttribute("Type", info.suffix().toUpper());
			p.setAttribute("Executable", info.isExecutable());
			p.setAttribute("Writable", info.isWritable());
			p.setAttribute("Readable", info.isReadable());
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
			p.setAttribute("Created", info.birthTime());
#else
			p.setAttribute("Created", info.created());
#endif
			p.setAttribute("Last modified", info.lastModified());
			p.setAttribute("Last read", info.lastRead());
		}

		res << p;
	}

	return res;
}

QIODevice* VipPhysicalFileSystem::openPath(const VipPath& path, QIODevice::OpenMode modes)
{
	QFile* file = new QFile(path.canonicalPath());
	if (file->open(modes))
		return file;
	else
		delete file;
	return nullptr;
}

#ifdef _WIN32

#include <atomic>
#include <qprocess.h>
#include <qthread.h>

#define _OPEN_ERROR(...)                                                                                                                                                                               \
	{                                                                                                                                                                                              \
		VIP_LOG_ERROR(__VA_ARGS__);                                                                                                                                                            \
		error = __VA_ARGS__;                                                                                                                                                                   \
		finished = true;                                                                                                                                                                       \
		proc.terminate();                                                                                                                                                                      \
		return;                                                                                                                                                                                \
	}

class VipSFTPFileSystem::PrivateData : public QThread
{
public:
	QString error;
	VipPath listPath;
	VipPath getPath;
	QString outFile;
	VipPathList result;
	QByteArray address;
	QByteArray password;
	QByteArray current;
	QByteArray root;
	QMutex mutex;
	std::atomic<bool> finished;
	std::atomic<bool> stop;

	// list of path -> attributes
	QMap<QString, QVariantMap> attributes;

	PrivateData()
	  : QThread()
	  , finished(false)
	  , stop(false)
	{
	}
	~PrivateData()
	{
		stop = true;
		wait();
	}

	QString readOutput(QProcess& proc)
	{
		QString res;
		while (true) {
			if (proc.waitForReadyRead(100)) {

				QString tmp = proc.readAllStandardOutput();
				if (tmp.isEmpty())
					return res;
				res += tmp;
				if (res.contains("psftp> "))
					break;
			}
			if (proc.state() != QProcess::Running)
				return QString();
		}
		res.remove("\npsftp> ");
		if (res.startsWith("\n"))
			res = res.mid(1);
		if (res.endsWith("\n"))
			res = res.mid(0, res.size() - 1);
		return res;
	}

	static bool waitForInput(QProcess& p)
	{
		while (true) {

			if (!p.waitForReadyRead(2000))
				return false;
			QString tmp = p.readAllStandardOutput();
			if (tmp.contains("psftp> "))
				return true;
			if (tmp.contains("closed", Qt::CaseInsensitive) || tmp.contains("error", Qt::CaseInsensitive))
				return false;
			if (tmp.contains("denied", Qt::CaseInsensitive))
				return false;

			if (p.state() != QProcess::Running)
				return false;
		}
	}

	virtual void run()
	{
		current.clear();
		root.clear();
		error.clear();

		QProcess proc;

		proc.setProcessChannelMode(QProcess::MergedChannels);

		QString cmd = "psftp"; //-pw " + password + "" + address;
		proc.start(cmd, QStringList() << "-pw" << password << address);
		if (!proc.waitForStarted(2000))
			_OPEN_ERROR("Unable to connect to " + address + ", please check address and password");

		if (proc.state() != QProcess::Running)
			_OPEN_ERROR("Unable to connect to " + address + ", please check address and password");

		proc.write("y\n");
		if (!proc.waitForBytesWritten(4000))
			_OPEN_ERROR("Unable to write to psftp process, please check address and password");

		if (!waitForInput(proc))
			_OPEN_ERROR("Unable to read from psftp process, please check address and password");

		proc.write("pwd\n");
		if (!proc.waitForBytesWritten(2000))
			_OPEN_ERROR("Unable to write to psftp process, please check address and password");

		QByteArray current_pwd = readOutput(proc).toLatin1();
		if (current_pwd.contains("not connected")) {
			_OPEN_ERROR("Connection error, please check address and password");
		}

		current_pwd.replace("Remote directory is ", "");
		current_pwd.replace("\\", "/");
		QList<QByteArray> lst = current_pwd.split('/');
		if (lst.size() && lst.first().isEmpty())
			root = "/";
		else if (lst.size())
			root = lst.first();

		current = current_pwd;
		finished = true;

		// start main loop
		while (!stop) {
			vipSleep(5);

			if (finished == false) {

				if (!getPath.isEmpty() && !outFile.isEmpty()) {
					// download file

					proc.write(("get " + getPath.canonicalPath() + " " + outFile + "\n").toLatin1());
					proc.waitForBytesWritten(2000);
					QString res = readOutput(proc);
					getPath = VipPath();
					outFile.clear();
					error.clear();
					res.replace("\n", "");
					if (!res.contains("=>"))
						error = "Unable get remote file " + getPath.canonicalPath() + ": " + res;

					finished = true;
				}
				if (!listPath.isEmpty()) {

					result.clear();
					if (proc.state() != QProcess::Running) {
						listPath = VipPath();
						finished = true;
						continue;
					}

					proc.write(("ls " + listPath.canonicalPath() + "\n").toLatin1());
					if (!proc.waitForBytesWritten(2000)) {
						listPath = VipPath();
						finished = true;
						continue;
					}

					QString tmp = readOutput(proc);
					QStringList _lst = tmp.split("\n");

					QString start = listPath.canonicalPath();
					if (!start.endsWith("/"))
						start += "/";

					for (const QString& str : _lst) {

						QStringList l = str.split(" ", Qt::SkipEmptyParts);
						if (l.size() < 5)
							continue;

						QString filename = l.back();
						filename = filename.mid(0, filename.size() - 1);
						if (filename == "." || filename == "..")
							continue;

						QString rights = l.first();
						QString size = l[4];
						bool is_dir = rights[0] == 'd' || rights[0] == 'l';
						bool can_read = rights[1] == 'r';
						bool can_write = rights[2] == 'w';
						bool can_exec = rights[3] == 'x';

						VipPath p(start + filename, is_dir);
						p.setAttribute("Size", size);
						p.setAttribute("Executable", can_exec);
						p.setAttribute("Writable", can_write);
						p.setAttribute("Readable", can_read);
						result.push_back(p);

						attributes.insert(p.canonicalPath(), p.attributes());
					}

					listPath = VipPath();
					finished = true;
				}
			}

			if (proc.state() != QProcess::Running)
				break;
		}

		if (proc.state() == QProcess::Running) {
			proc.write("quit\n");
			proc.waitForBytesWritten(2000);
			if (!proc.waitForFinished(2000))
				proc.terminate();
		}
		finished = true;
	}
};

VipSFTPFileSystem::VipSFTPFileSystem()
  : VipMapFileSystem(OpenRead | OpenText)
{
	d_data = new PrivateData();
}

VipSFTPFileSystem::~VipSFTPFileSystem()
{
	delete d_data;
}

bool VipSFTPFileSystem::open(const QByteArray& addr)
{
	QMutexLocker lock(&d_data->mutex);

	if (d_data->isRunning()) {
		d_data->stop = true;
		d_data->wait();
	}

	d_data->current.clear();
	d_data->root.clear();
	d_data->address = addr;
	d_data->stop = false;
	d_data->finished = false;
	d_data->start();

	// wait for connected
	while (!d_data->finished) {
		vipSleep(5);
	}

	if (!d_data->error.isEmpty()) {
		return false;
	}
	return true;
}
QByteArray VipSFTPFileSystem::address() const
{
	return d_data->address;
}
bool VipSFTPFileSystem::isOpen() const
{
	return d_data->isRunning();
}

void VipSFTPFileSystem::setPassword(const QByteArray& pwd)
{
	d_data->password = pwd;
}

QStringList VipSFTPFileSystem::standardFileSystemAttributes()
{
	static QStringList attrs = QStringList() << "Size"
						 << "Executable"
						 << "Writable"
						 << "Readable";
	return attrs;
}
VipPathList VipSFTPFileSystem::rootPaths()
{
	if (!d_data->root.isEmpty())
		return VipPathList() << VipPath(QString(d_data->root), true);
	return VipPathList();
}
bool VipSFTPFileSystem::pathExists(const VipPath& path)
{
	(void)path;
	return true;
}
bool VipSFTPFileSystem::createPath(const VipPath&)
{
	return false;
}
bool VipSFTPFileSystem::removePath(const VipPath&)
{
	return false;
}
bool VipSFTPFileSystem::renamePath(const VipPath&, const VipPath&)
{
	return false;
}
bool VipSFTPFileSystem::copyPath(const VipPath& src, const VipPath& dst)
{
	(void)src;
	(void)dst;
	return false;
}
VipPathList VipSFTPFileSystem::listPathContent(const VipPath& path)
{
	QMutexLocker lock(&d_data->mutex);

	if (!isOpen())
		return VipPathList();

	d_data->listPath = path;
	d_data->finished = false;
	while (!d_data->finished) {
		vipSleep(5);
	}
	return d_data->result;
}
QIODevice* VipSFTPFileSystem::openPath(const VipPath& path, QIODevice::OpenMode modes)
{

	if (!(modes & QIODevice::ReadOnly))
		return nullptr;

	// build temporary filename
	QString tmp = vipGetTempDirectory();
	if (!tmp.endsWith("/"))
		tmp += "/";

	QString fname = tmp + QFileInfo(path.canonicalPath()).fileName();

	QMutexLocker lock(&d_data->mutex);

	qint64 outsize = d_data->attributes[path.canonicalPath()]["Size"].toLongLong();

	d_data->error.clear();
	if (outsize == 0 || outsize != QFileInfo(fname).size()) {

		d_data->getPath = path;
		d_data->outFile = fname;
		d_data->finished = false;

		VipProgress p;
		p.setRange(0, outsize);
		p.setText("<b>Load file </b>" + QFileInfo(path.canonicalPath()).fileName());
		while (!d_data->finished) {

			p.setValue(QFileInfo(fname).size());
			vipSleep(5);
		}
	}

	if (!d_data->error.isEmpty()) {
		VIP_LOG_ERROR(d_data->error);
		return nullptr;
	}

	QFile* file = new QFile(fname);
	if (!file->open(QFile::ReadOnly)) {
		VIP_LOG_ERROR("Unable to open file ", fname);
		return nullptr;
	}
	return file;
}

#endif

#include "VipArchive.h"

static VipArchive& operator<<(VipArchive& arch, VipMapFileSystem* sys)
{
	arch.content("address", sys->address());
	arch.content("alias", sys->objectName());
	arch.content("operations", (int)sys->supportedOperations());
	arch.content("id", VipUniqueId::id(sys));
	return arch;
}

static VipArchive& operator>>(VipArchive& arch, VipMapFileSystem* sys)
{
	QByteArray address = arch.read("address").toByteArray();
	QString alias = arch.read("alias").toString();
	int operations = arch.read("operations").toInt();
	int id = arch.read("id").toInt();

	sys->setSupportedOperations(VipMapFileSystem::SupportedOperations(operations));
	sys->open(address);
	sys->setObjectName(alias);
	VipUniqueId::setId(sys, id);
	return arch;
}

static int registerVipMapFileSystem()
{
	vipRegisterArchiveStreamOperators<VipMapFileSystem*>();
	return 0;
}

static int _registerVipMapFileSystem = registerVipMapFileSystem();
