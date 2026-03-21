#ifndef AIS_N2K_SENDERS_H_
#define AIS_N2K_SENDERS_H_

#include <N2kMessages.h>

#include <ctime>

#include "N2kMsg.h"
#include "ais/ais_conversions.h"
#include "ais/ais_message_types.h"
#include "sensesp/system/lambda_consumer.h"

namespace ais {

class AISClassAPositionN2kSender
    : public sensesp::ValueConsumer<ClassAPositionReport> {
 public:
  AISClassAPositionN2kSender(tNMEA2000* nmea2000) : nmea2000_(nmea2000) {}

  void set(const ClassAPositionReport& r) override {
    tN2kMsg msg;
    SetN2kPGN129038(
        msg, r.message_id,
        static_cast<tN2kAISRepeat>(r.repeat & 0x03), r.mmsi, r.latitude,
        r.longitude, r.accuracy, r.raim, r.timestamp,
        cog_to_radians(r.cog), sog_to_ms(r.sog),
        N2kaischannel_A_VDL_reception, heading_to_radians(r.heading),
        rot_to_rads(r.rot), static_cast<tN2kAISNavStatus>(r.nav_status));
    nmea2000_->SendMsg(msg);
  }

 private:
  tNMEA2000* nmea2000_;
};

class AISClassBPositionN2kSender
    : public sensesp::ValueConsumer<ClassBPositionReport> {
 public:
  AISClassBPositionN2kSender(tNMEA2000* nmea2000) : nmea2000_(nmea2000) {}

  void set(const ClassBPositionReport& r) override {
    tN2kMsg msg;
    SetN2kPGN129039(
        msg, r.message_id,
        static_cast<tN2kAISRepeat>(r.repeat & 0x03), r.mmsi, r.latitude,
        r.longitude, r.accuracy, r.raim, r.timestamp,
        cog_to_radians(r.cog), sog_to_ms(r.sog),
        N2kaischannel_A_VDL_reception, heading_to_radians(r.heading),
        r.cs_unit ? N2kaisunit_ClassB_CS : N2kaisunit_ClassB_SOTDMA,
        r.display, r.dsc, r.band, r.msg22,
        r.assigned ? N2kaismode_Assigned : N2kaismode_Autonomous, false);
    nmea2000_->SendMsg(msg);
  }

 private:
  tNMEA2000* nmea2000_;
};

class AISClassAStaticN2kSender
    : public sensesp::ValueConsumer<ClassAStaticData> {
 public:
  AISClassAStaticN2kSender(tNMEA2000* nmea2000) : nmea2000_(nmea2000) {}

  void set(const ClassAStaticData& d) override {
    auto dims = dimensions_to_n2k(d.to_bow, d.to_stern, d.to_port,
                                   d.to_starboard);

    // ETA requires a valid system clock to resolve the year.
    // Before GNSS time sync, use N2K "not available" values.
    constexpr time_t kMinValidTime = 1704067200;  // 2024-01-01T00:00:00Z
    uint16_t eta_date = N2kUInt16NA;
    double eta_time = N2kDoubleNA;

    time_t now = time(nullptr);
    if (now >= kMinValidTime) {
      struct tm* tm_now = gmtime(&now);
      uint16_t current_year =
          static_cast<uint16_t>(tm_now->tm_year + 1900);
      auto eta = eta_to_n2k(d.month, d.day, d.hour, d.minute, current_year);
      eta_date = eta.date;
      eta_time = eta.time;
    }

    tN2kMsg msg;
    SetN2kPGN129794(
        msg, d.message_id,
        static_cast<tN2kAISRepeat>(d.repeat & 0x03), d.mmsi, d.imo,
        d.callsign, d.name, d.ship_type, dims.length, dims.beam,
        dims.pos_ref_stbd, dims.pos_ref_bow, eta_date, eta_time,
        d.draught, d.destination,
        static_cast<tN2kAISVersion>(d.ais_version),
        static_cast<tN2kGNSStype>(d.epfd),
        d.dte ? N2kaisdte_NotReady : N2kaisdte_Ready);
    nmea2000_->SendMsg(msg);
  }

 private:
  tNMEA2000* nmea2000_;
};

class AISSafetyMessageN2kSender
    : public sensesp::ValueConsumer<SafetyMessage> {
 public:
  AISSafetyMessageN2kSender(tNMEA2000* nmea2000) : nmea2000_(nmea2000) {}

  void set(const SafetyMessage& m) override {
    tN2kMsg msg;
    SetN2kPGN129802(msg, m.message_id,
                    static_cast<tN2kAISRepeat>(m.repeat & 0x03), m.mmsi,
                    N2kaischannel_A_VDL_reception, m.text);
    nmea2000_->SendMsg(msg);
  }

 private:
  tNMEA2000* nmea2000_;
};

class AISClassBStaticN2kSender
    : public sensesp::ValueConsumer<ClassBStaticData> {
 public:
  AISClassBStaticN2kSender(tNMEA2000* nmea2000) : nmea2000_(nmea2000) {}

  void set(const ClassBStaticData& d) override {
    if (d.part_number == 0) {
      // Part A: PGN 129809
      tN2kMsg msg;
      SetN2kPGN129809(msg, d.message_id,
                      static_cast<tN2kAISRepeat>(d.repeat & 0x03), d.mmsi,
                      d.name);
      nmea2000_->SendMsg(msg);
    } else if (d.part_number == 1) {
      // Part B: PGN 129810
      auto dims = dimensions_to_n2k(d.to_bow, d.to_stern, d.to_port,
                                     d.to_starboard);
      tN2kMsg msg;
      SetN2kPGN129810(msg, d.message_id,
                      static_cast<tN2kAISRepeat>(d.repeat & 0x03), d.mmsi,
                      d.ship_type, d.vendor_id, d.callsign, dims.length,
                      dims.beam, dims.pos_ref_stbd, dims.pos_ref_bow,
                      d.mothership_mmsi);
      nmea2000_->SendMsg(msg);
    }
  }

 private:
  tNMEA2000* nmea2000_;
};

class AISAtoNN2kSender : public sensesp::ValueConsumer<AtoNReport> {
 public:
  AISAtoNN2kSender(tNMEA2000* nmea2000) : nmea2000_(nmea2000) {}

  void set(const AtoNReport& r) override {
    tN2kAISAtoNReportData data;
    data.MessageID = r.message_id;
    data.Repeat = static_cast<tN2kAISRepeat>(r.repeat & 0x03);
    data.UserID = r.mmsi;
    data.Latitude = r.latitude;
    data.Longitude = r.longitude;
    data.Accuracy = r.accuracy;
    data.RAIM = r.raim;
    data.Seconds = r.timestamp;
    auto dims =
        dimensions_to_n2k(r.to_bow, r.to_stern, r.to_port, r.to_starboard);
    data.Length = dims.length;
    data.Beam = dims.beam;
    data.PositionReferenceStarboard = dims.pos_ref_stbd;
    data.PositionReferenceTrueNorth = dims.pos_ref_bow;
    data.AtoNType = static_cast<tN2kAISAtoNType>(r.aton_type);
    data.OffPositionIndicator = r.off_position;
    data.VirtualAtoNFlag = r.virtual_aton;
    data.AssignedModeFlag = r.assigned;
    data.GNSSType = static_cast<tN2kGNSStype>(r.epfd);
    data.AtoNStatus = r.aton_status;
    data.AISTransceiverInformation = N2kaischannel_A_VDL_reception;
    data.SetAtoNName(r.name);

    tN2kMsg msg;
    SetN2kPGN129041(msg, data);
    nmea2000_->SendMsg(msg);
  }

 private:
  tNMEA2000* nmea2000_;
};

}  // namespace ais

#endif  // AIS_N2K_SENDERS_H_
