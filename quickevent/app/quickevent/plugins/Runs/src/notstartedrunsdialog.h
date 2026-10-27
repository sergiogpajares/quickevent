#pragma once

#include "runsplugin.h"

#include <QDialog>

class QCheckBox;
class QListWidget;
class QTableWidget;
class QSpinBox;
class QLabel;
class QPushButton;

namespace Runs {

/// Finds runners of a stage without any sign that they started (no check, readout, punch, ...)
/// and lets the user mark them as DNS. The user chooses which records count as a sign of start.
class NotStartedRunsDialog : public QDialog
{
	Q_OBJECT
private:
	using Super = QDialog;
public:
	NotStartedRunsDialog(int stage_id, int preselected_class_id, QWidget *parent = nullptr);

	int markedCount() const { return m_markedCount; }
private:
	void loadClasses(int preselected_class_id);
	void setAllClassesChecked(bool checked);
	QList<int> checkedClassIds() const;
	RunsPlugin::StartEvidence startEvidence() const;
	void findRuns();
	QList<int> checkedRunIds() const;
	void updateStatus();
	void markDns();
private:
	int m_stageId;
	int m_markedCount = 0;
	QListWidget *m_lstClasses;
	struct EvidenceCheckBox { QCheckBox *checkBox; bool RunsPlugin::StartEvidence::*field; };
	QList<EvidenceCheckBox> m_evidenceCheckBoxes;
	QPushButton *m_btFind;
	QSpinBox *m_edCheckBeforeStartMin;
	QTableWidget *m_tblRuns;
	QLabel *m_lblStatus;
	QPushButton *m_btMarkDns;
};

} // namespace Runs
