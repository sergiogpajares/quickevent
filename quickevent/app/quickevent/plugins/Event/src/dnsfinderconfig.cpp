#include "dnsfinderconfig.h"

namespace Event {

namespace {
struct EvidenceKey { const char *key; bool StartEvidence::*field; };
constexpr EvidenceKey EVIDENCE_KEYS[] = {
	{"checkTime", &StartEvidence::checkTime},
	{"clearTime", &StartEvidence::clearTime},
	{"finishTime", &StartEvidence::finishTime},
	{"cardReadout", &StartEvidence::cardReadout},
	{"radioPunches", &StartEvidence::radioPunches},
	{"backupCheck", &StartEvidence::backupCheck},
	{"backupClear", &StartEvidence::backupClear},
	{"backupStart", &StartEvidence::backupStart},
	{"backupOther", &StartEvidence::backupOther},
};
constexpr auto CHECK_BEFORE_START_MIN = "checkBeforeStartMin";
}

DnsFinderConfig DnsFinderConfig::fromVariantMap(const QVariantMap &map)
{
	DnsFinderConfig config;
	for(const auto &[key, field] : EVIDENCE_KEYS)
		config.evidence.*field = map.value(key, config.evidence.*field).toBool();
	config.checkBeforeStartMin = map.value(CHECK_BEFORE_START_MIN, config.checkBeforeStartMin).toInt();
	return config;
}

QVariantMap DnsFinderConfig::toVariantMap() const
{
	QVariantMap ret;
	for(const auto &[key, field] : EVIDENCE_KEYS)
		ret[key] = evidence.*field;
	ret[CHECK_BEFORE_START_MIN] = checkBeforeStartMin;
	return ret;
}

} // namespace Event
