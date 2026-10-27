#include "notstartedrunsdialog.h"
#include "runsplugin.h"

#include <quickevent/core/og/timems.h>

#include <qf/gui/framework/mainwindow.h>
#include <qf/gui/dialogs/messagebox.h>
#include <qf/core/exception.h>
#include <qf/core/sql/query.h>

#include <QCheckBox>
#include <QColor>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

using qf::gui::framework::getPlugin;

namespace Runs {

namespace {
enum Column { ColClass = 0, ColName, ColRegistration, ColSiId, ColStart, ColNote, ColCount };
}

NotStartedRunsDialog::NotStartedRunsDialog(int stage_id, int preselected_class_id, QWidget *parent)
	: Super(parent)
	, m_stageId(stage_id)
{
	setWindowTitle(tr("Runners without check - stage %1").arg(stage_id));
	resize(900, 600);

	auto *main_layout = new QVBoxLayout(this);
	{
		auto *lbl = new QLabel(tr("Finds running competitors with none of the ticked signs of having started. "
								  "Selected runners will be marked as DNS.\n"
								  "When more runners share an SI card, its records are matched to them by start time. "
								  "Runners whose records cannot be matched reliably are labelled for manual check and not ticked."));
		lbl->setWordWrap(true);
		main_layout->addWidget(lbl);
	}
	auto *body_layout = new QHBoxLayout();
	main_layout->addLayout(body_layout, 1);
	{
		auto *ly = new QVBoxLayout();
		ly->addWidget(new QLabel(tr("Classes")));
		m_lstClasses = new QListWidget();
		ly->addWidget(m_lstClasses, 1);
		auto *ly_bt = new QHBoxLayout();
		auto *bt_all = new QPushButton(tr("All"));
		connect(bt_all, &QPushButton::clicked, this, [this]() { setAllClassesChecked(true); });
		ly_bt->addWidget(bt_all);
		auto *bt_none = new QPushButton(tr("None"));
		connect(bt_none, &QPushButton::clicked, this, [this]() { setAllClassesChecked(false); });
		ly_bt->addWidget(bt_none);
		ly->addLayout(ly_bt);

		auto *gb = new QGroupBox(tr("Count as started"));
		auto *ly_gb = new QVBoxLayout(gb);
		using E = RunsPlugin::StartEvidence;
		auto add_check_box = [this, ly_gb](const QString &caption, const QString &tool_tip, bool E::*field) {
			auto *cb = new QCheckBox(caption);
			cb->setToolTip(tool_tip);
			cb->setChecked(E{}.*field);
			connect(cb, &QCheckBox::toggled, this, &NotStartedRunsDialog::updateStatus);
			ly_gb->addWidget(cb);
			m_evidenceCheckBoxes << EvidenceCheckBox{cb, field};
		};
		add_check_box(tr("Check time"), tr("Check time in the runs table"), &E::checkTime);
		add_check_box(tr("Clear time"), tr("Clear time in the runs table"), &E::clearTime);
		add_check_box(tr("Finish time"), tr("Finish time in the runs table"), &E::finishTime);
		add_check_box(tr("Card readout"), tr("SI card read out at finish"), &E::cardReadout);
		add_check_box(tr("Radio punches"), tr("Punches received online, e.g. from SRR"), &E::radioPunches);
		add_check_box(tr("Backup: check stations"), tr("Station backup memory records of check stations"), &E::backupCheck);
		add_check_box(tr("Backup: clear stations"), tr("Station backup memory records of clear stations"), &E::backupClear);
		add_check_box(tr("Backup: start stations"), tr("Station backup memory records of start stations"), &E::backupStart);
		add_check_box(tr("Backup: other stations"), tr("Station backup memory records of finish and control stations and of stations with unknown mode"), &E::backupOther);
		ly->addWidget(gb);
		body_layout->addLayout(ly);
	}
	{
		auto *ly = new QVBoxLayout();
		auto *ly_opts = new QHBoxLayout();
		auto *lbl_check = new QLabel(tr("Shared cards: check punched at most"));
		ly_opts->addWidget(lbl_check);
		m_edCheckBeforeStartMin = new QSpinBox();
		m_edCheckBeforeStartMin->setRange(1, 120);
		m_edCheckBeforeStartMin->setValue(10);
		m_edCheckBeforeStartMin->setSuffix(tr(" min"));
		m_edCheckBeforeStartMin->setToolTip(tr("Used to match records of a shared SI card to its runners by start time"));
		lbl_check->setBuddy(m_edCheckBeforeStartMin);
		ly_opts->addWidget(m_edCheckBeforeStartMin);
		ly_opts->addWidget(new QLabel(tr("before start")));
		ly_opts->addStretch();
		m_btFind = new QPushButton(tr("&Find runners"));
		connect(m_btFind, &QPushButton::clicked, this, &NotStartedRunsDialog::findRuns);
		ly_opts->addWidget(m_btFind);
		ly->addLayout(ly_opts);

		m_tblRuns = new QTableWidget(0, ColCount);
		m_tblRuns->setHorizontalHeaderLabels({tr("Class"), tr("Name"), tr("Registration"), tr("SI"), tr("Start"), tr("Note")});
		m_tblRuns->setEditTriggers(QAbstractItemView::NoEditTriggers);
		m_tblRuns->setSelectionBehavior(QAbstractItemView::SelectRows);
		m_tblRuns->verticalHeader()->setVisible(false);
		m_tblRuns->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
		connect(m_tblRuns, &QTableWidget::itemChanged, this, &NotStartedRunsDialog::updateStatus);
		ly->addWidget(m_tblRuns, 1);

		m_lblStatus = new QLabel();
		ly->addWidget(m_lblStatus);
		body_layout->addLayout(ly, 3);
	}
	{
		auto *ly_bt = new QHBoxLayout();
		ly_bt->addStretch();
		m_btMarkDns = new QPushButton(tr("&Mark checked as DNS"));
		connect(m_btMarkDns, &QPushButton::clicked, this, &NotStartedRunsDialog::markDns);
		ly_bt->addWidget(m_btMarkDns);
		auto *bt_close = new QPushButton(tr("Close"));
		connect(bt_close, &QPushButton::clicked, this, &QDialog::reject);
		ly_bt->addWidget(bt_close);
		main_layout->addLayout(ly_bt);
	}
	loadClasses(preselected_class_id);
	updateStatus();
}

void NotStartedRunsDialog::loadClasses(int preselected_class_id)
{
	auto add_item = [this, preselected_class_id](const QString &caption, int class_id) {
		auto *it = new QListWidgetItem(caption, m_lstClasses);
		it->setData(Qt::UserRole, class_id);
		it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
		bool checked = preselected_class_id <= 0 || preselected_class_id == class_id;
		it->setCheckState(checked? Qt::Checked: Qt::Unchecked);
	};
	qf::core::sql::Query q;
	q.execThrow("SELECT id, name FROM classes ORDER BY name");
	while(q.next())
		add_item(q.value(1).toString(), q.value(0).toInt());
	q.execThrow("SELECT COUNT(*) FROM runs JOIN competitors ON competitors.id=runs.competitorId"
				" WHERE competitors.classId IS NULL AND runs.stageId=" + QString::number(m_stageId));
	if(q.next() && q.value(0).toInt() > 0)
		add_item(tr("--- no class ---"), 0);
}

void NotStartedRunsDialog::setAllClassesChecked(bool checked)
{
	for(int i = 0; i < m_lstClasses->count(); i++)
		m_lstClasses->item(i)->setCheckState(checked? Qt::Checked: Qt::Unchecked);
}

QList<int> NotStartedRunsDialog::checkedClassIds() const
{
	QList<int> ret;
	for(int i = 0; i < m_lstClasses->count(); i++) {
		auto *it = m_lstClasses->item(i);
		if(it->checkState() == Qt::Checked)
			ret << it->data(Qt::UserRole).toInt();
	}
	return ret;
}

RunsPlugin::StartEvidence NotStartedRunsDialog::startEvidence() const
{
	RunsPlugin::StartEvidence ret;
	for(const auto &ecb : m_evidenceCheckBoxes)
		ret.*ecb.field = ecb.checkBox->isChecked();
	return ret;
}

void NotStartedRunsDialog::findRuns()
{
	m_tblRuns->blockSignals(true);
	m_tblRuns->setRowCount(0);
	m_tblRuns->blockSignals(false);
	const QList<int> class_ids = checkedClassIds();
	if(class_ids.isEmpty()) {
		updateStatus();
		return;
	}
	QList<RunsPlugin::NotStartedRun> runs;
	try {
		runs = getPlugin<RunsPlugin>()->runsWithoutStartEvidence(m_stageId, class_ids, startEvidence()
																 , qint64(m_edCheckBeforeStartMin->value()) * 60 * 1000);
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
		return;
	}
	m_tblRuns->blockSignals(true);
	m_tblRuns->setRowCount(static_cast<int>(runs.size()));
	for(int row = 0; row < runs.size(); row++) {
		const auto &r = runs[row];
		auto *it_class = new QTableWidgetItem(r.className);
		it_class->setFlags(it_class->flags() | Qt::ItemIsUserCheckable);
		const bool manual_check = r.sharedCard == RunsPlugin::NotStartedRun::SharedCard::ManualCheck;
		it_class->setCheckState(manual_check? Qt::Unchecked: Qt::Checked);
		it_class->setData(Qt::UserRole, r.runId);
		m_tblRuns->setItem(row, ColClass, it_class);
		m_tblRuns->setItem(row, ColName, new QTableWidgetItem(r.competitorName));
		m_tblRuns->setItem(row, ColRegistration, new QTableWidgetItem(r.registration));
		m_tblRuns->setItem(row, ColSiId, new QTableWidgetItem(r.siId > 0? QString::number(r.siId): QString()));
		QString start = r.startTimeMs.isNull()? QString(): quickevent::core::og::TimeMs(r.startTimeMs.toInt()).toString();
		m_tblRuns->setItem(row, ColStart, new QTableWidgetItem(start));
		QString note;
		if(r.sharedCard == RunsPlugin::NotStartedRun::SharedCard::TimeMatched)
			note = tr("Shared card - matched by time");
		else if(manual_check)
			note = tr("Shared card - manual check required");
		auto *it_note = new QTableWidgetItem(note);
		m_tblRuns->setItem(row, ColNote, it_note);
		if(manual_check) {
			for(int col = 0; col < ColCount; col++)
				m_tblRuns->item(row, col)->setBackground(QColor(255, 200, 120));
		}
	}
	m_tblRuns->blockSignals(false);
	m_tblRuns->resizeColumnToContents(ColClass);
	m_tblRuns->resizeColumnToContents(ColNote);
	updateStatus();
}

QList<int> NotStartedRunsDialog::checkedRunIds() const
{
	QList<int> ret;
	for(int row = 0; row < m_tblRuns->rowCount(); row++) {
		auto *it = m_tblRuns->item(row, ColClass);
		if(it && it->checkState() == Qt::Checked)
			ret << it->data(Qt::UserRole).toInt();
	}
	return ret;
}

void NotStartedRunsDialog::updateStatus()
{
	const auto checked_cnt = checkedRunIds().size();
	m_lblStatus->setText(tr("Found %1 runners without check, %2 checked to be marked as DNS.")
						 .arg(m_tblRuns->rowCount()).arg(checked_cnt));
	m_btMarkDns->setEnabled(checked_cnt > 0);
	// without any sign of start every runner would be found
	m_btFind->setEnabled(startEvidence().isAny());
}

void NotStartedRunsDialog::markDns()
{
	const QList<int> run_ids = checkedRunIds();
	if(run_ids.isEmpty())
		return;
	if(!qf::gui::dialogs::MessageBox::askYesNo(this, tr("Mark %1 runners as DNS?").arg(run_ids.size())))
		return;
	try {
		m_markedCount += getPlugin<RunsPlugin>()->markRunsNotStarted(run_ids, this);
	}
	catch (const qf::core::Exception &e) {
		qf::gui::dialogs::MessageBox::showException(this, e);
		return;
	}
	// marked runners are not candidates any more
	findRuns();
}

} // namespace Runs
