#pragma once
#include "job_options.h"
#include <qfile.h>

class ListOfJobOptions : public QObject {
  Q_OBJECT

protected:
  ~ListOfJobOptions() = default;
  ListOfJobOptions();

public:
  static ListOfJobOptions *getInstance();
  bool Persist(JobOptions *jo);
  bool Forget(JobOptions *jo);
  QList<JobOptions *> &getTasks() { return tasks; }
  // loads the tasks file into dataIn (public so that a test can reload it)
  static bool RestoreFromUserData(ListOfJobOptions &dataIn);

signals:
  void tasksListUpdated();

private:
  static ListOfJobOptions *SavedJobOptions;
  static const QString persistenceFileName;
  static QString GetPersistenceFilePath();
  static QFile *GetPersistenceFile(QIODevice::OpenModeFlag mode);

  QList<JobOptions *> tasks;
  bool PersistToUserData();
};
