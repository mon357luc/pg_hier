-- Additional setup script for testing extensions
-- This demonstrates that setup-test-db will run all setup*.sql files

-- Create a simple test table for extension validation
CREATE TABLE IF NOT EXISTS test_extensions (
    extension_name VARCHAR(100) PRIMARY KEY,
    installed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    notes TEXT
);

-- Insert record showing pg_hier was set up
INSERT INTO test_extensions (extension_name, notes) 
VALUES ('pg_hier', 'Test setup completed') 
ON CONFLICT (extension_name) DO UPDATE SET 
    installed_at = CURRENT_TIMESTAMP,
    notes = 'Test setup re-run at ' || CURRENT_TIMESTAMP;
