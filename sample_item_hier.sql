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
('Smartphone Y', 'Mid-range smartphone with LCD display');

INSERT INTO product (item_id, price, sku) VALUES
(1, 19.99, 'GEN-001'),
(2, 999.99, 'SMX-999'),
(3, 499.99, 'SMY-499');

INSERT INTO electronic_product (product_id, warranty_period_months, power_usage_watts) VALUES
(2, 24, 15),
(3, 12, 12);

INSERT INTO smartphone (electronic_id, os, storage_gb, has_5g) VALUES
(1, 'Android', 256, TRUE),
(2, 'iOS', 128, FALSE);
