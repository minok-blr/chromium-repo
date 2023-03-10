// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_SENDER_OPENSCREEN_FRAME_SENDER_H_
#define MEDIA_CAST_SENDER_OPENSCREEN_FRAME_SENDER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/sender/congestion_control.h"
#include "media/cast/sender/frame_sender.h"

#include "third_party/openscreen/src/cast/streaming/sender.h"

namespace media::cast {

struct SenderEncodedFrame;

// This is the Open Screen implementation of the FrameSender API. It wraps
// an openscreen::cast::Sender object and provides some basic functionality
// that is shared between the AudioSender, VideoSender, and RemotingSender
// classes.
//
// For more information, see the Cast Streaming README.md located at:
// https://source.ch40m1um.qjz9zk/chromium/chromium/src/+/main:third_party/openscreen/src/cast/streaming/README.md

class OpenscreenFrameSender : public FrameSender,
                              openscreen::cast::Sender::Observer {
 public:
  // TODO(https://crbug.com/1318499): will likely need to remove
  // FrameSenderConfig here once the migration to libcast is complete.
  OpenscreenFrameSender(scoped_refptr<CastEnvironment> cast_environment,
                        const FrameSenderConfig& config,
                        openscreen::cast::Sender* sender,
                        Client& client);
  ~OpenscreenFrameSender() override;

  // FrameSender overrides.
  void SetTargetPlayoutDelay(base::TimeDelta new_target_playout_delay) override;
  base::TimeDelta GetTargetPlayoutDelay() const override;
  bool NeedsKeyFrame() const override;
  void EnqueueFrame(std::unique_ptr<SenderEncodedFrame> encoded_frame) override;
  bool ShouldDropNextFrame(base::TimeDelta frame_duration) const override;
  RtpTimeTicks GetRecordedRtpTimestamp(FrameId frame_id) const override;
  int GetUnacknowledgedFrameCount() const override;
  int GetSuggestedBitrate(base::TimeTicks playout_time,
                          base::TimeDelta playout_delay) override;
  double MaxFrameRate() const override;
  void SetMaxFrameRate(double max_frame_rate) override;
  base::TimeDelta TargetPlayoutDelay() const override;
  base::TimeDelta CurrentRoundTripTime() const override;
  base::TimeTicks LastSendTime() const override;
  FrameId LatestAckedFrameId() const override;

 private:
  // TODO(https://crbug.com/1318499): these should be removed from the
  // FrameSender API.
  void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) override;
  void OnReceivedPli() override;
  void OnMeasuredRoundTripTime(base::TimeDelta rtt) override;

  // openscreen::cast::Sender::Observer overrides.
  void OnFrameCanceled(openscreen::cast::FrameId frame_id) override;
  void OnPictureLost() override;

  // Helper for getting the reference time recorded on the frame associated
  // with |frame_id|.
  base::TimeTicks GetRecordedReferenceTime(FrameId frame_id) const;

  // Record timestamps for later retrieval by GetRecordedRtpTimestamp.
  void RecordLatestFrameTimestamps(FrameId frame_id,
                                   base::TimeTicks reference_time,
                                   RtpTimeTicks rtp_timestamp);

  base::TimeDelta GetInFlightMediaDuration() const;

 private:
  // Returns the maximum media duration currently allowed in-flight.  This
  // fluctuates in response to the currently-measured network latency.
  base::TimeDelta GetAllowedInFlightMediaDuration() const;

  // The cast environment.
  const scoped_refptr<CastEnvironment> cast_environment_;

  // The backing Open Screen sender implementation.
  raw_ptr<openscreen::cast::Sender> const sender_;

  // The frame sender client.
  Client& client_;

  // Max encoded frames generated per second.
  double max_frame_rate_;

  // Whether this is an audio or video frame sender.
  const bool is_audio_;

  // The congestion control manages frame statistics and helps make decisions
  // about what bitrate we encode the next frame at.
  std::unique_ptr<CongestionControl> congestion_control_;

  // The target playout delay, may fluctuate between the min and max delays.
  base::TimeDelta target_playout_delay_;
  base::TimeDelta min_playout_delay_;
  base::TimeDelta max_playout_delay_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last frame sent.  This member is invalid until
  // |!last_send_time_.is_null()|.
  FrameId last_sent_frame_id_;

  // This is the maximum delay that the sender should get ack from receiver.
  // Counts how many RTCP reports are being "aggressively" sent (i.e., one per
  // frame) at the start of the session.  Once a threshold is reached, RTCP
  // reports are instead sent at the configured interval + random drift.
  int num_aggressive_rtcp_reports_sent_ = 0;

  // Should send the target playout delay with the next frame. Behind the
  // scenes, the openscreen::cast::Sender checks the frame's playout delay and
  // notifies the receiver if it has changed.
  bool send_target_playout_delay_ = false;

  // Ring buffer to keep track of recent frame timestamps. These should only be
  // accessed through the Record/GetXXX() methods.  The index into this ring
  // buffer is the lower 8 bits of the FrameId.
  RtpTimeTicks frame_rtp_timestamps_[256];

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<OpenscreenFrameSender> weak_factory_{this};
};

}  // namespace media::cast

#endif  // MEDIA_CAST_SENDER_OPENSCREEN_FRAME_SENDER_H_
