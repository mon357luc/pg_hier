-- Create the taxon schema
CREATE SCHEMA taxon;

-- Set the search path to use the taxon schema
SET search_path TO taxon, public;

-- Create separate tables for each taxonomic rank
CREATE TABLE kingdoms (
    kingdom_id SERIAL PRIMARY KEY,
    scientific_name VARCHAR(100) NOT NULL UNIQUE,
    common_name VARCHAR(100),
    description TEXT
);

CREATE TABLE phyla (
    phylum_id SERIAL PRIMARY KEY,
    scientific_name VARCHAR(100) NOT NULL,
    common_name VARCHAR(100),
    kingdom_id INTEGER NOT NULL REFERENCES kingdoms(kingdom_id),
    description TEXT,
    UNIQUE (scientific_name, kingdom_id)
);

CREATE TABLE classes (
    class_id SERIAL PRIMARY KEY,
    scientific_name VARCHAR(100) NOT NULL,
    common_name VARCHAR(100),
    phylum_id INTEGER NOT NULL REFERENCES phyla(phylum_id),
    description TEXT,
    UNIQUE (scientific_name, phylum_id)
);

CREATE TABLE orders (
    order_id SERIAL PRIMARY KEY,
    scientific_name VARCHAR(100) NOT NULL,
    common_name VARCHAR(100),
    class_id INTEGER NOT NULL REFERENCES classes(class_id),
    description TEXT,
    UNIQUE (scientific_name, class_id)
);

CREATE TABLE families (
    family_id SERIAL PRIMARY KEY,
    scientific_name VARCHAR(100) NOT NULL,
    common_name VARCHAR(100),
    order_id INTEGER NOT NULL REFERENCES orders(order_id),
    description TEXT,
    UNIQUE (scientific_name, order_id)
);

CREATE TABLE genera (
    genus_id SERIAL PRIMARY KEY,
    scientific_name VARCHAR(100) NOT NULL,
    common_name VARCHAR(100),
    family_id INTEGER NOT NULL REFERENCES families(family_id),
    description TEXT,
    UNIQUE (scientific_name, family_id)
);

CREATE TABLE species (
    species_id SERIAL PRIMARY KEY,
    scientific_name VARCHAR(100) NOT NULL,
    common_name VARCHAR(100),
    genus_id INTEGER NOT NULL REFERENCES genera(genus_id),
    description TEXT,
    UNIQUE (scientific_name, genus_id)
);

-- Insert sample data
-- First, insert kingdoms
INSERT INTO kingdoms (scientific_name, common_name, description) VALUES
('Animalia', 'Animals', 'Multicellular eukaryotic organisms that form the kingdom Animalia'),
('Plantae', 'Plants', 'Multicellular organisms that form the kingdom Plantae'),
('Fungi', 'Fungi', 'Kingdom of eukaryotic organisms that includes yeasts, molds, and mushrooms');

-- Insert phyla
INSERT INTO phyla (scientific_name, common_name, kingdom_id, description) VALUES
('Chordata', 'Chordates', 1, 'Animals with a notochord'),
('Arthropoda', 'Arthropods', 1, 'Animals with exoskeletons, segmented bodies, and paired jointed appendages'),
('Magnoliophyta', 'Flowering plants', 2, 'Plants that produce flowers'),
('Pinophyta', 'Conifers', 2, 'Cone-bearing seed plants'),
('Basidiomycota', 'Club fungi', 3, 'Phylum of fungi that includes mushrooms');

-- Insert classes
INSERT INTO classes (scientific_name, common_name, phylum_id, description) VALUES
('Mammalia', 'Mammals', 1, 'Vertebrates that possess mammary glands and hair'),
('Aves', 'Birds', 1, 'Warm-blooded vertebrates with feathers'),
('Reptilia', 'Reptiles', 1, 'Cold-blooded vertebrates with scales'),
('Amphibia', 'Amphibians', 1, 'Vertebrates that can live on land and in water'),
('Insecta', 'Insects', 2, 'Arthropods with a three-part body, three pairs of jointed legs, and usually wings'),
('Arachnida', 'Arachnids', 2, 'Arthropods with eight legs'),
('Magnoliopsida', 'Dicotyledons', 3, 'Flowering plants with two embryonic leaves'),
('Liliopsida', 'Monocotyledons', 3, 'Flowering plants with one embryonic leaf'),
('Agaricomycetes', 'Mushrooms', 5, 'Class of fungi that forms mushrooms');

-- Insert orders
INSERT INTO orders (scientific_name, common_name, class_id, description) VALUES
('Carnivora', 'Carnivores', 1, 'Mammals that primarily eat meat'),
('Primates', 'Primates', 1, 'Mammals including monkeys, apes, and humans'),
('Rodentia', 'Rodents', 1, 'Mammals with continuously growing incisors'),
('Passeriformes', 'Perching birds', 2, 'Birds with feet adapted for perching'),
('Squamata', 'Scaled reptiles', 3, 'Reptiles with scales'),
('Anura', 'Frogs and toads', 4, 'Order of amphibians without tails'),
('Coleoptera', 'Beetles', 5, 'Insects with hardened forewings'),
('Araneae', 'Spiders', 6, 'Arachnids that spin silk webs'),
('Rosales', 'Roses and allies', 7, 'Order of flowering plants'),
('Agaricales', 'Gilled mushrooms', 9, 'Order of mushroom-forming fungi');

-- Insert families
INSERT INTO families (scientific_name, common_name, order_id, description) VALUES
('Felidae', 'Cats', 1, 'Family of carnivorous mammals'),
('Canidae', 'Dogs', 1, 'Family of carnivorous mammals'),
('Hominidae', 'Great apes', 2, 'Family of primates'),
('Muridae', 'Mice and rats', 3, 'Family of rodents'),
('Corvidae', 'Crows and jays', 4, 'Family of perching birds'),
('Paridae', 'Tits and chickadees', 4, 'Family of small passerine birds'),
('Colubridae', 'Colubrids', 5, 'Family of snakes'),
('Ranidae', 'True frogs', 6, 'Family of frogs'),
('Scarabaeidae', 'Scarab beetles', 7, 'Family of beetles'),
('Formicidae', 'Ants', 7, 'Family of social insects'),
('Araneidae', 'Orb-weaver spiders', 8, 'Family of spiders'),
('Rosaceae', 'Rose family', 9, 'Family of flowering plants'),
('Fagaceae', 'Beech family', 9, 'Family of trees including beeches and oaks'),
('Pinaceae', 'Pine family', 9, 'Family of coniferous trees'),
('Amanitaceae', 'Amanita family', 10, 'Family of mushrooms');

-- Insert genera
INSERT INTO genera (scientific_name, common_name, family_id, description) VALUES
('Felis', 'Small cats', 1, 'Genus of small cats'),
('Panthera', 'Big cats', 1, 'Genus of big cats'),
('Canis', 'Wolves and dogs', 2, 'Genus including wolves and dogs'),
('Homo', 'Humans', 3, 'Genus of humans'),
('Pan', 'Chimpanzees', 3, 'Genus of chimpanzees'),
('Mus', 'Mice', 4, 'Genus of mice'),
('Rattus', 'Rats', 4, 'Genus of rats'),
('Corvus', 'Crows', 5, 'Genus of crows'),
('Parus', 'Tits', 6, 'Genus of tits'),
('Python', 'Pythons', 7, 'Genus of pythons'),
('Rana', 'True frogs', 8, 'Genus of frogs'),
('Scarabaeus', 'Scarab beetles', 9, 'Genus of scarab beetles'),
('Formica', 'Wood ants', 10, 'Genus of ants'),
('Araneus', 'Orb-weaver spiders', 11, 'Genus of orb-weaver spiders'),
('Rosa', 'Roses', 12, 'Genus of roses'),
('Quercus', 'Oaks', 13, 'Genus of trees and shrubs'),
('Pinus', 'Pines', 14, 'Genus of coniferous trees'),
('Amanita', 'Amanita', 15, 'Genus of mushrooms');

-- Insert species
-- Species for Felis
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Felis catus', 'Domestic cat', 1, 'Common household pet'),
('Felis silvestris', 'Wildcat', 1, 'Small wild cat native to Europe, Africa and Asia'),
('Felis margarita', 'Sand cat', 1, 'Small wild cat living in deserts'),
('Felis nigripes', 'Black-footed cat', 1, 'Small wild cat of southern Africa');

-- Species for Panthera
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Panthera leo', 'Lion', 2, 'Large cat native to Africa'),
('Panthera tigris', 'Tiger', 2, 'Largest cat species'),
('Panthera onca', 'Jaguar', 2, 'Large cat native to the Americas'),
('Panthera pardus', 'Leopard', 2, 'Spotted big cat widely distributed across Africa and Asia');

-- Species for Canis
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Canis lupus', 'Gray wolf', 3, 'Largest wild canid'),
('Canis familiaris', 'Domestic dog', 3, 'Domesticated form of the wolf'),
('Canis latrans', 'Coyote', 3, 'Canid native to North America'),
('Canis aureus', 'Golden jackal', 3, 'Wolf-like canid native to north Africa, southeastern Europe, and south Asia');

-- Species for Homo
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Homo sapiens', 'Human', 4, 'Modern humans'),
('Homo neanderthalensis', 'Neanderthal', 4, 'Extinct species of archaic human'),
('Homo erectus', 'Upright man', 4, 'Extinct species of archaic human');

-- Species for Pan
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Pan troglodytes', 'Common chimpanzee', 5, 'Great ape native to the forest of tropical Africa'),
('Pan paniscus', 'Bonobo', 5, 'Great ape native to the Congo Basin');

-- Species for Mus
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Mus musculus', 'House mouse', 6, 'Small rodent, typically a house pest'),
('Mus caroli', 'Ryukyu mouse', 6, 'Mouse native to Southeast Asia'),
('Mus cookii', 'Cook''s mouse', 6, 'Mouse native to South and Southeast Asia'),
('Mus cervicolor', 'Fawn-colored mouse', 6, 'Mouse found in South and Southeast Asia');

-- Species for Rattus
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Rattus norvegicus', 'Brown rat', 7, 'Common rat found worldwide'),
('Rattus rattus', 'Black rat', 7, 'Common long-tailed rodent'),
('Rattus exulans', 'Pacific rat', 7, 'Rat native to Southeast Asia'),
('Rattus tanezumi', 'Asian house rat', 7, 'Rat common in Asia');

-- Species for Corvus
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Corvus corax', 'Common raven', 8, 'Largest perching bird'),
('Corvus corone', 'Carrion crow', 8, 'Crow found across Europe and eastern Asia'),
('Corvus brachyrhynchos', 'American crow', 8, 'Crow native to North America'),
('Corvus frugilegus', 'Rook', 8, 'Crow found in Europe and Asia');

-- Species for Parus
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Parus major', 'Great tit', 9, 'Common garden bird in Europe and Asia'),
('Parus caeruleus', 'Blue tit', 9, 'Small blue and yellow bird');

-- Species for Python
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Python reticulatus', 'Reticulated python', 10, 'Longest snake species'),
('Python molurus', 'Indian python', 10, 'Large python native to tropical South and Southeast Asia'),
('Python regius', 'Ball python', 10, 'Python species native to West and Central Africa'),
('Python sebae', 'African rock python', 10, 'Large python native to sub-Saharan Africa');

-- Species for Rana
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Rana temporaria', 'European common frog', 11, 'Frog common in Europe'),
('Rana catesbeiana', 'American bullfrog', 11, 'Large frog native to North America');

-- Species for Scarabaeus
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Scarabaeus sacer', 'Sacred scarab', 12, 'Dung beetle revered by ancient Egyptians'),
('Scarabaeus laticollis', 'Mediterranean scarab', 12, 'Scarab beetle found in the Mediterranean region'),
('Scarabaeus semipunctatus', 'Coastal scarab', 12, 'Scarab beetle found on sandy coasts'),
('Scarabaeus pius', 'Pious scarab', 12, 'Scarab beetle from southern Europe');

-- Species for Formica
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Formica rufa', 'Red wood ant', 13, 'Large red and black ant'),
('Formica polyctena', 'European red wood ant', 13, 'Wood ant native to Europe');

-- Species for Araneus
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Araneus diadematus', 'European garden spider', 14, 'Common orb-weaver spider'),
('Araneus cavaticus', 'Barn spider', 14, 'Orb-weaver spider found in North America'),
('Araneus quadratus', 'Four-spot orb-weaver', 14, 'Spider with distinctive markings'),
('Araneus marmoreus', 'Marbled orb-weaver', 14, 'Colorful orb-weaver spider');

-- Species for Rosa
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Rosa canina', 'Dog rose', 15, 'Wild climbing rose native to Europe'),
('Rosa rugosa', 'Rugosa rose', 15, 'Rose native to eastern Asia'),
('Rosa gallica', 'Gallic rose', 15, 'Rose native to southern and central Europe'),
('Rosa chinensis', 'China rose', 15, 'Rose native to Southwest China');

-- Species for Quercus
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Quercus robur', 'English oak', 16, 'Common oak tree in Europe'),
('Quercus alba', 'White oak', 16, 'Oak native to eastern North America'),
('Quercus rubra', 'Red oak', 16, 'Oak native to North America');

-- Species for Pinus
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Pinus sylvestris', 'Scots pine', 17, 'Coniferous tree native to Eurasia'),
('Pinus radiata', 'Monterey pine', 17, 'Pine native to California'),
('Pinus nigra', 'Black pine', 17, 'Pine native to southern Europe');

-- Species for Amanita
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Amanita muscaria', 'Fly amanita', 18, 'Red mushroom with white spots'),
('Amanita phalloides', 'Death cap', 18, 'Deadly poisonous mushroom'),
('Amanita caesarea', 'Caesar''s mushroom', 18, 'Edible mushroom');

-- Add more species to reach around 100 total records
-- Additional species for existing genera
INSERT INTO species (scientific_name, common_name, genus_id, description) VALUES
('Felis chaus', 'Jungle cat', 1, 'Wild cat native to Asia'),
('Felis manul', 'Pallas''s cat', 1, 'Small wild cat native to Central Asia'),
('Panthera uncia', 'Snow leopard', 2, 'Large cat native to mountain ranges of Central Asia'),
('Canis adustus', 'Side-striped jackal', 3, 'Jackal native to central and southern Africa'),
('Canis mesomelas', 'Black-backed jackal', 3, 'Jackal native to eastern and southern Africa'),
('Homo floresiensis', 'Flores man', 4, 'Extinct species of small archaic humans'),
('Mus spicilegus', 'Mound-building mouse', 6, 'Mouse that builds mounds of vegetation'),
('Rattus argentiventer', 'Rice-field rat', 7, 'Rat found in rice fields in Asia'),
('Rattus nitidus', 'Himalayan rat', 7, 'Rat found in the Himalayan region'),
('Corvus monedula', 'Jackdaw', 8, 'Small crow-like bird'),
('Corvus splendens', 'House crow', 8, 'Crow common in urban areas in Asia'),
('Parus ater', 'Coal tit', 9, 'Small passerine bird'),
('Python bivittatus', 'Burmese python', 10, 'One of the largest snakes in the world'),
('Rana pipiens', 'Northern leopard frog', 11, 'Frog with distinctive spots'),
('Rana palustris', 'Pickerel frog', 11, 'Frog with rectangular spots'),
('Araneus trifolium', 'Shamrock spider', 14, 'Large orb-weaver spider'),
('Rosa moschata', 'Musk rose', 15, 'Rose with musky scent'),
('Rosa multiflora', 'Multiflora rose', 15, 'Rose that produces clusters of flowers'),
('Quercus ilex', 'Holm oak', 16, 'Evergreen oak native to the Mediterranean'),
('Quercus suber', 'Cork oak', 16, 'Oak known for its thick, corky bark'),
('Pinus pinaster', 'Maritime pine', 17, 'Pine native to the Mediterranean'),
('Pinus pinea', 'Stone pine', 17, 'Pine that produces edible pine nuts'),
('Amanita verna', 'Fool''s mushroom', 18, 'Deadly poisonous white mushroom'),
('Amanita citrina', 'False death cap', 18, 'Yellow-tinged mushroom');