DROP TABLE IF EXISTS order_items CASCADE;
DROP TABLE IF EXISTS orders CASCADE;
DROP TABLE IF EXISTS customers CASCADE;
DROP TABLE IF EXISTS products CASCADE;
DROP TABLE IF EXISTS product_categories CASCADE;
DROP TABLE IF EXISTS stores CASCADE;

CREATE TABLE stores (
    store_id SERIAL PRIMARY KEY,
    store_name VARCHAR(100) NOT NULL,
    store_url VARCHAR(255),
    description TEXT,
    established_date DATE,
    country VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE product_categories (
    category_id SERIAL PRIMARY KEY,
    category_name VARCHAR(100) NOT NULL,
    parent_category_id INTEGER REFERENCES product_categories(category_id),
    category_path VARCHAR(500), 
    level_depth INTEGER DEFAULT 0,
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200) NOT NULL,
    category_id INTEGER REFERENCES product_categories(category_id),
    store_id INTEGER REFERENCES stores(store_id),
    sku VARCHAR(50) UNIQUE,
    description TEXT,
    price DECIMAL(10,2) NOT NULL,
    cost DECIMAL(10,2),
    stock_quantity INTEGER DEFAULT 0,
    weight_kg DECIMAL(8,3),
    dimensions_cm VARCHAR(50),
    brand VARCHAR(100),
    model VARCHAR(100),
    color VARCHAR(50),
    size VARCHAR(20),
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE customers (
    customer_id SERIAL,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    phone VARCHAR(20),
    date_of_birth DATE,
    address_line1 VARCHAR(100),
    address_line2 VARCHAR(100),
    city VARCHAR(50),
    state_province VARCHAR(50),
    postal_code VARCHAR(20),
    country VARCHAR(50),
    registration_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    is_active BOOLEAN DEFAULT true,
    PRIMARY KEY (customer_id, first_name, last_name)
);

CREATE TABLE customer_statistics 
(
    customer_id INTEGER,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    total_orders INTEGER DEFAULT 0,
    total_spent DECIMAL(10,2) DEFAULT 0.00,
    average_order_value DECIMAL(10,2) DEFAULT 0.00,
    last_order_date TIMESTAMP,
    PRIMARY KEY (customer_id, first_name, last_name),
    FOREIGN KEY (customer_id, first_name, last_name) REFERENCES customers(customer_id, first_name, last_name)
);

CREATE TABLE orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INTEGER,
    customer_first_name VARCHAR(50),
    customer_last_name VARCHAR(50),
    store_id INTEGER REFERENCES stores(store_id),
    order_number VARCHAR(50) UNIQUE NOT NULL,
    order_status VARCHAR(20) DEFAULT 'pending', -- pending, confirmed, shipped, delivered, cancelled
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    shipping_date TIMESTAMP,
    delivery_date TIMESTAMP,
    subtotal DECIMAL(10,2) NOT NULL,
    tax_amount DECIMAL(10,2) DEFAULT 0,
    shipping_cost DECIMAL(10,2) DEFAULT 0,
    discount_amount DECIMAL(10,2) DEFAULT 0,
    total_amount DECIMAL(10,2) NOT NULL,
    payment_method VARCHAR(50), -- credit_card, debit_card, paypal, bank_transfer
    payment_status VARCHAR(20) DEFAULT 'pending', -- pending, paid, failed, refunded
    shipping_address_line1 VARCHAR(100),
    shipping_address_line2 VARCHAR(100),
    shipping_city VARCHAR(50),
    shipping_state_province VARCHAR(50),
    shipping_postal_code VARCHAR(20),
    shipping_country VARCHAR(50),
    notes TEXT,
    FOREIGN KEY (customer_id, customer_first_name, customer_last_name) REFERENCES customers(customer_id, first_name, last_name)
);

CREATE TABLE order_items (
    order_item_id SERIAL PRIMARY KEY,
    order_id INTEGER REFERENCES orders(order_id),
    product_id INTEGER REFERENCES products(product_id),
    quantity INTEGER NOT NULL DEFAULT 1,
    unit_price DECIMAL(10,2) NOT NULL,
    discount_per_item DECIMAL(10,2) DEFAULT 0,
    line_total DECIMAL(10,2) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert sample stores
INSERT INTO stores (store_name, store_url, description, established_date, country) VALUES
('TechMart Online', 'www.techmart.com', 'Leading electronics and technology retailer', '2010-03-15', 'USA'),
('Fashion Galaxy', 'www.fashiongalaxy.com', 'Premium fashion and lifestyle store', '2015-07-22', 'UK'),
('Home & Garden Paradise', 'www.homegardenparadise.com', 'Everything for your home and garden needs', '2012-11-08', 'Canada'),
('Sports World', 'www.sportsworld.com', 'Athletic gear and sports equipment', '2008-05-30', 'Australia'),
('Global Marketplace', 'www.globalmarketplace.com', 'Multi-vendor marketplace platform', '2005-01-01', 'USA');

-- Insert hierarchical product categories
INSERT INTO product_categories (category_name, parent_category_id, category_path, level_depth, description) VALUES
-- Root categories (level 0)
('Electronics', NULL, 'Electronics', 0, 'All electronic devices and accessories'),
('Clothing & Fashion', NULL, 'Clothing & Fashion', 0, 'Apparel and fashion accessories'),
('Home & Garden', NULL, 'Home & Garden', 0, 'Home improvement and garden supplies'),
('Sports & Outdoors', NULL, 'Sports & Outdoors', 0, 'Sporting goods and outdoor equipment'),
('Books & Media', NULL, 'Books & Media', 0, 'Books, movies, music, and digital media');

-- Level 1 subcategories for Electronics
INSERT INTO product_categories (category_name, parent_category_id, category_path, level_depth, description) VALUES
('Computers & Tablets', 1, 'Electronics > Computers & Tablets', 1, 'Desktop computers, laptops, and tablets'),
('Mobile Phones', 1, 'Electronics > Mobile Phones', 1, 'Smartphones and basic mobile phones'),
('Audio & Video', 1, 'Electronics > Audio & Video', 1, 'Headphones, speakers, cameras, and entertainment devices'),
('Gaming', 1, 'Electronics > Gaming', 1, 'Video game consoles, games, and accessories'),
('Smart Home', 1, 'Electronics > Smart Home', 1, 'IoT devices and home automation products');

-- Level 2 subcategories for Computers & Tablets
INSERT INTO product_categories (category_name, parent_category_id, category_path, level_depth, description) VALUES
('Laptops', 6, 'Electronics > Computers & Tablets > Laptops', 2, 'Portable computers and notebooks'),
('Desktop Computers', 6, 'Electronics > Computers & Tablets > Desktop Computers', 2, 'Tower and all-in-one desktop systems'),
('Tablets', 6, 'Electronics > Computers & Tablets > Tablets', 2, 'Tablet computers and e-readers'),
('Computer Accessories', 6, 'Electronics > Computers & Tablets > Computer Accessories', 2, 'Keyboards, mice, monitors, and other peripherals');

-- Level 1 subcategories for Clothing & Fashion
INSERT INTO product_categories (category_name, parent_category_id, category_path, level_depth, description) VALUES
('Men''s Clothing', 2, 'Clothing & Fashion > Men''s Clothing', 1, 'Clothing and accessories for men'),
('Women''s Clothing', 2, 'Clothing & Fashion > Women''s Clothing', 1, 'Clothing and accessories for women'),
('Children''s Clothing', 2, 'Clothing & Fashion > Children''s Clothing', 1, 'Clothing for kids and babies'),
('Shoes', 2, 'Clothing & Fashion > Shoes', 1, 'Footwear for all ages and occasions'),
('Accessories', 2, 'Clothing & Fashion > Accessories', 1, 'Jewelry, bags, belts, and other fashion accessories');

-- Insert sample products
INSERT INTO products (product_name, category_id, store_id, sku, description, price, cost, stock_quantity, weight_kg, brand, model, color, is_active) VALUES
-- Electronics
('MacBook Pro 16-inch', 11, 1, 'MBP-16-001', 'Apple MacBook Pro with M2 chip, 16GB RAM, 512GB SSD', 2499.99, 1899.99, 25, 2.15, 'Apple', 'MacBook Pro 16"', 'Space Gray', true),
('Dell XPS 13', 11, 1, 'DELL-XPS13-001', 'Dell XPS 13 ultrabook with Intel i7, 16GB RAM, 256GB SSD', 1299.99, 999.99, 15, 1.27, 'Dell', 'XPS 13', 'Silver', true),
('iPhone 15 Pro', 7, 1, 'IPH-15PRO-001', 'Apple iPhone 15 Pro with 128GB storage', 999.99, 749.99, 50, 0.187, 'Apple', 'iPhone 15 Pro', 'Natural Titanium', true),
('Samsung Galaxy S24', 7, 1, 'SAM-S24-001', 'Samsung Galaxy S24 with 256GB storage', 799.99, 599.99, 35, 0.167, 'Samsung', 'Galaxy S24', 'Phantom Black', true),
('iPad Air', 13, 1, 'IPAD-AIR-001', 'Apple iPad Air with M1 chip, 64GB Wi-Fi', 599.99, 449.99, 40, 0.461, 'Apple', 'iPad Air', 'Space Gray', true),
('Sony WH-1000XM5', 8, 1, 'SONY-WH1000XM5-001', 'Sony wireless noise-canceling headphones', 399.99, 299.99, 30, 0.25, 'Sony', 'WH-1000XM5', 'Black', true),
('PlayStation 5', 9, 1, 'PS5-001', 'Sony PlayStation 5 gaming console', 499.99, 399.99, 10, 4.5, 'Sony', 'PlayStation 5', 'White', true),

-- Clothing
('Levi''s 501 Jeans', 15, 2, 'LEVI-501-001', 'Classic Levi''s 501 original fit jeans', 89.99, 45.00, 100, 0.6, 'Levi''s', '501 Original', 'Indigo', true),
('Nike Air Max 270', 18, 4, 'NIKE-AM270-001', 'Nike Air Max 270 running shoes', 149.99, 89.99, 75, 0.8, 'Nike', 'Air Max 270', 'White/Black', true),
('Patagonia Down Jacket', 15, 4, 'PAT-DOWN-001', 'Patagonia lightweight down jacket', 229.99, 149.99, 20, 0.45, 'Patagonia', 'Down Sweater', 'Navy Blue', true),

-- Home & Garden
('KitchenAid Stand Mixer', 3, 3, 'KA-MIXER-001', 'KitchenAid Artisan 5-quart stand mixer', 379.99, 249.99, 12, 10.9, 'KitchenAid', 'Artisan 5-Qt', 'Empire Red', true),
('Dyson V15 Vacuum', 3, 3, 'DYS-V15-001', 'Dyson V15 Detect cordless vacuum cleaner', 749.99, 549.99, 8, 3.1, 'Dyson', 'V15 Detect', 'Yellow/Iron', true);

-- Insert sample customers
INSERT INTO customers (first_name, last_name, email, phone, address_line1, city, state_province, postal_code, country) VALUES
('John', 'Smith', 'john.smith@email.com', '+1-555-0101', '123 Main Street', 'New York', 'NY', '10001', 'USA'),
('Emily', 'Johnson', 'emily.johnson@email.com', '+1-555-0102', '456 Oak Avenue', 'Los Angeles', 'CA', '90210', 'USA'),
('Michael', 'Brown', 'michael.brown@email.com', '+44-20-7946-0103', '789 High Street', 'London', 'England', 'SW1A 1AA', 'UK'),
('Sarah', 'Davis', 'sarah.davis@email.com', '+1-416-555-0104', '321 Maple Drive', 'Toronto', 'ON', 'M5V 3A4', 'Canada'),
('David', 'Wilson', 'david.wilson@email.com', '+61-2-9374-0105', '654 George Street', 'Sydney', 'NSW', '2000', 'Australia');

--Insert sample customer statistics
INSERT INTO customer_statistics (customer_id, first_name, last_name, total_orders, total_spent, average_order_value, last_order_date) VALUES
(1, 'John', 'Smith', 5, 2699.98, 539.996, '2024-04-01'),
(2, 'Emily', 'Johnson', 3, 2121.96, 707.32, '2024-04-02'),
(3, 'Michael', 'Brown', 2, 107.18, 53.59, '2024-04-03'),
(4, 'Sarah', 'Davis', 1, 167.98, 167.98, '2024-04-04'),
(5, 'David', 'Wilson', 1, 1245.38, 1245.38, '2024-04-05');

-- Insert sample orders
INSERT INTO orders (customer_id, customer_first_name, customer_last_name, store_id, order_number, order_status, subtotal, tax_amount, shipping_cost, total_amount, payment_method, payment_status, shipping_address_line1, shipping_city, shipping_state_province, shipping_postal_code, shipping_country) VALUES
(1, 'John', 'Smith', 1, 'ORD-2024-001', 'delivered', 2499.99, 199.99, 0.00, 2699.98, 'credit_card', 'paid', '123 Main Street', 'New York', 'NY', '10001', 'USA'),
(2, 'Emily', 'Johnson', 1, 'ORD-2024-002', 'shipped', 1949.98, 155.99, 15.99, 2121.96, 'paypal', 'paid', '456 Oak Avenue', 'Los Angeles', 'CA', '90210', 'USA'),
(3, 'Michael', 'Brown', 2, 'ORD-2024-003', 'confirmed', 89.99, 7.20, 9.99, 107.18, 'credit_card', 'paid', '789 High Street', 'London', 'England', 'SW1A 1AA', 'UK'),
(1, 'John', 'Smith', 4, 'ORD-2024-004', 'pending', 149.99, 12.00, 5.99, 167.98, 'debit_card', 'pending', '123 Main Street', 'New York', 'NY', '10001', 'USA'),
(4, 'Sarah', 'Davis', 3, 'ORD-2024-005', 'delivered', 1129.98, 90.40, 25.00, 1245.38, 'credit_card', 'paid', '321 Maple Drive', 'Toronto', 'ON', 'M5V 3A4', 'Canada');

-- Insert order items
INSERT INTO order_items (order_id, product_id, quantity, unit_price, line_total) VALUES
(1, 1, 1, 2499.99, 2499.99),

(2, 3, 1, 999.99, 999.99),
(2, 6, 1, 399.99, 399.99),
(2, 5, 1, 599.99, 599.99),

(3, 8, 1, 89.99, 89.99),

(4, 9, 1, 149.99, 149.99),

(5, 11, 1, 379.99, 379.99),
(5, 12, 1, 749.99, 749.99);

CREATE INDEX idx_products_category_id ON products(category_id);
CREATE INDEX idx_products_store_id ON products(store_id);
CREATE INDEX idx_orders_customer_composite ON orders(customer_id, customer_first_name, customer_last_name);
CREATE INDEX idx_orders_store_id ON orders(store_id);
CREATE INDEX idx_orders_order_date ON orders(order_date);
CREATE INDEX idx_order_items_order_id ON order_items(order_id);
CREATE INDEX idx_order_items_product_id ON order_items(product_id);
CREATE INDEX idx_product_categories_parent_id ON product_categories(parent_category_id);

CREATE VIEW product_hierarchy_view AS
SELECT 
    p.product_id,
    p.product_name,
    p.sku,
    p.price,
    p.stock_quantity,
    s.store_name,
    pc.category_name,
    pc.category_path,
    pc.level_depth
FROM products p
JOIN stores s ON p.store_id = s.store_id
JOIN product_categories pc ON p.category_id = pc.category_id
WHERE p.is_active = true;

-- Create a view for order items with detailed information
CREATE VIEW order_items_detail_view AS
SELECT 
    oi.order_item_id,
    oi.order_id,
    oi.product_id,
    p.product_name,
    p.sku,
    p.brand,
    p.model,
    p.color,
    oi.quantity,
    oi.unit_price,
    p.price AS current_price,
    oi.discount_per_item,
    oi.line_total,
    (oi.unit_price - p.cost) * oi.quantity AS profit_margin,
    CASE 
        WHEN oi.unit_price < p.price THEN 'Discounted'
        WHEN oi.unit_price = p.price THEN 'Regular Price'
        ELSE 'Premium Price'
    END AS pricing_status,
    oi.created_at AS item_added_date
FROM order_items oi
JOIN products p ON oi.product_id = p.product_id;

CREATE VIEW order_summary_view AS
SELECT 
    o.order_id,
    o.order_number,
    o.order_status,
    o.order_date,
    o.customer_first_name || ' ' || o.customer_last_name AS customer_name,
    c.email AS customer_email,
    s.store_name,
    o.total_amount,
    COUNT(oi.order_item_id) AS total_items,
    SUM(oi.quantity) AS total_quantity
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id AND o.customer_first_name = c.first_name AND o.customer_last_name = c.last_name
JOIN stores s ON o.store_id = s.store_id
LEFT JOIN order_items oi ON o.order_id = oi.order_id
GROUP BY o.order_id, o.order_number, o.order_status, o.order_date, 
         o.customer_first_name, o.customer_last_name, c.email, s.store_name, o.total_amount;

SELECT 'Stores created:' AS summary, COUNT(*) AS count FROM stores
UNION ALL
SELECT 'Product categories created:', COUNT(*) FROM product_categories
UNION ALL
SELECT 'Products created:', COUNT(*) FROM products
UNION ALL
SELECT 'Customers created:', COUNT(*) FROM customers
UNION ALL
SELECT 'Orders created:', COUNT(*) FROM orders
UNION ALL
SELECT 'Order items created:', COUNT(*) FROM order_items;
