#pragma once

#include <QVariantMap>

namespace Event {

/// Which records count as a sign that the runner started
struct StartEvidence
{
	bool checkTime = true; ///< runs.checkTimeMs
	bool clearTime = true; ///< runs.clearTimeMs
	bool finishTime = true; ///< runs.finishTimeMs
	bool cardReadout = true; ///< cards
	bool radioPunches = true; ///< punches
	bool backupCheck = true; ///< stationsbackup of check stations
	bool backupClear = true; ///< stationsbackup of clear stations
	bool backupStart = true; ///< stationsbackup of start stations
	bool backupOther = true; ///< stationsbackup of other stations: finish, control or unknown mode

	bool isAnyBackup() const { return backupCheck || backupClear || backupStart || backupOther; }
	bool isAny() const { return checkTime || clearTime || finishTime || cardReadout || radioPunches || isAnyBackup(); }
};

/// Options of the Find DNS runners dialog, stored in the event config table
struct DnsFinderConfig
{
	static DnsFinderConfig fromVariantMap(const QVariantMap &map);
	QVariantMap toVariantMap() const;

	StartEvidence evidence;
	int checkBeforeStartMin = 10; ///< used to match records of a shared SI card to its runners
};

} // namespace Event
