-- SignalRoute — Geofence Expiry
-- Add expiry support and soft-delete tracking.

ALTER TABLE geofence_rules
    ADD COLUMN expires_at TIMESTAMPTZ,
    ADD COLUMN deleted_at TIMESTAMPTZ;

-- Index for expired fence cleanup
CREATE INDEX idx_fence_expiry
    ON geofence_rules (expires_at)
    WHERE expires_at IS NOT NULL AND active = true;
