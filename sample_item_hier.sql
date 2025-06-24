CREATE TABLE item (
    item_id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT
);

CREATE TABLE product (
    product_id SERIAL PRIMARY KEY,
    item_id INT NOT NULL REFERENCES item(item_id) ON DELETE CASCADE,
    price NUMERIC(10, 2) NOT NULL,
    sku TEXT UNIQUE
);

CREATE TABLE electronic_product (
    electronic_id SERIAL PRIMARY KEY,
    product_id INT NOT NULL REFERENCES product(product_id) ON DELETE CASCADE,
    warranty_period_months INT,
    power_usage_watts INT
);

CREATE TABLE smartphone (
    smartphone_id SERIAL PRIMARY KEY,
    electronic_id INT NOT NULL REFERENCES electronic_product(electronic_id) ON DELETE CASCADE,
    os TEXT,
    storage_gb INT,
    has_5g BOOLEAN
);

CREATE INDEX idx_item_name ON item(name);
CREATE INDEX idx_product_sku ON product(sku);
CREATE INDEX idx_electronic_warranty ON electronic_product(warranty_period_months);
CREATE INDEX idx_smartphone_os ON smartphone(os);
CREATE INDEX idx_smartphone_storage ON smartphone(storage_gb);
CREATE INDEX idx_smartphone_5g ON smartphone(has_5g);

INSERT INTO item (name, description) VALUES
('Generic Item', 'A basic item'),
('Smartphone X', 'High-end smartphone with OLED display'),
('Smartphone Y', 'Mid-range smartphone with LCD display'),
('Tablet Z', '10-inch tablet with stylus support'),
('Laptop Pro', 'High-performance laptop for professionals'),
('Smartwatch Alpha', 'Wearable device with fitness tracking'),
('Bluetooth Speaker', 'Portable wireless speaker'),
('E-Reader', 'Lightweight e-reader with backlight');

INSERT INTO product (item_id, price, sku) VALUES
(1, 19.99, 'GEN-001'),
(2, 999.99, 'SMX-999'),
(3, 499.99, 'SMY-499'),
(4, 399.99, 'TAB-Z-400'),
(5, 1499.99, 'LTP-PRO-1500'),
(6, 199.99, 'SWT-ALPHA-200'),
(7, 59.99, 'BT-SPK-060'),
(8, 129.99, 'EREAD-130');

INSERT INTO electronic_product (product_id, warranty_period_months, power_usage_watts) VALUES
(2, 24, 15),
(3, 12, 12),
(4, 18, 10),
(5, 36, 65),
(6, 12, 5),
(7, 6, 8),
(8, 24, 3);

INSERT INTO smartphone (electronic_id, os, storage_gb, has_5g) VALUES
(1, 'Android', 256, TRUE),
(2, 'iOS', 128, FALSE),
(3, 'Android', 64, TRUE),
(4, 'Android', 32, FALSE);
