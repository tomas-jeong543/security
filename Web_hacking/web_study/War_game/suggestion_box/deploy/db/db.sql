CREATE USER 'chall'@'%' IDENTIFIED BY 'password';
CREATE DATABASE post_db CHARACTER SET utf8;
GRANT ALL PRIVILEGES ON post_db.* TO 'chall'@'%';

USE `post_db`;

CREATE TABLE IF NOT EXISTS articles (
    id INT AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    author VARCHAR(100) NOT NULL,
    content TEXT,
    is_private TINYINT(1) DEFAULT 0,
    password VARCHAR(255) DEFAULT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO articles (title, author, content, is_private, password)
VALUES (
  'Add advanced search filters and highlight matches',
  'Emily Carter',
  'The current search returns too many unrelated results. Please add advanced filters (date range, category, tag) and an option to sort by relevance or newest. Also consider highlighting the matched terms in search results and providing autosuggest as users type — this would greatly improve discoverability for suggestions.',
  0,
  NULL
);

INSERT INTO articles (title, author, content, is_private, password)
VALUES (
  'Confidential: Billing workflow edge-case causing chargebacks',
  'Michael Lee',
  'We have observed multiple chargebacks related to prorated refunds when customers cancel mid-subscription cycle. It appears the refund calculation does not account for promotional credits, which leads to disputes. I recommend an internal review of the billing logic and adding a reconciliation step to flag mismatches before refunds are issued. Please keep this note internal to finance/engineering teams. For verification during the internal exercise, here is a flag: DH{flag}.',
  1,
  'fakepasswd'
);
