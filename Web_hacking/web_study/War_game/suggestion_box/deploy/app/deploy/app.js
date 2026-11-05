const path = require('path');
const express = require('express');
const mysql = require('mysql2');
const util = require('util');

const app = express();
const PORT = process.env.PORT || 3000;

app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, 'views'));

app.use(express.urlencoded({ extended: false }));
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

const db = mysql.createConnection({
  host: 'db',
  user: 'chall',
  password: 'password',
  database: 'post_db'
});

const dbQuery = util.promisify(db.query).bind(db);

function escapeAndFormat(str) {
  if (!str) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;')
    .replace(/\r\n/g, '\n')
    .replace(/\n/g, '<br>');
}

function pad(n) { return String(n).padStart(2, '0'); }

function formatDateWithoutGMT(d) {
  if (!d) return '';
  const weekday = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'][d.getDay()];
  const month = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'][d.getMonth()];
  const day = pad(d.getDate());
  const year = d.getFullYear();
  const hh = pad(d.getHours());
  const mm = pad(d.getMinutes());
  const ss = pad(d.getSeconds());
  return `${weekday} ${month} ${day} ${year} ${hh}:${mm}:${ss}`;
}

app.get('/', async (req, res) => {
  try {
    const q = 'SELECT id, title, author, is_private, created_at FROM articles ORDER BY created_at DESC';
    const rows = await dbQuery(q);
    const articles = (rows || []).map(a => ({
      ...a,
      created_at: a.created_at ? formatDateWithoutGMT(new Date(a.created_at)) : ''
    }));
    res.render('index', { articles });
  } catch (err) {
    console.error('Error fetching articles:', err);
    res.status(500).send('Server error');
  }
});

app.get('/article/new', (req, res) => {
  res.render('new', { error: null, form: { title: '', author: '', content: '', is_private: false } });
});

app.post('/article/new', async (req, res) => {
  const { title, author, content, is_private, password } = req.body;

  if (!title || !author) {
    return res.render('new', { error: 'Title and author are required.', form: { title, author, content, is_private } });
  }

  const privateFlag = (is_private === 'on' || is_private === true || is_private === '1') ? 1 : 0;

  if (privateFlag && (!password || password.trim() === '')) {
    return res.render('new', { error: 'Password is required for private articles.', form: { title, author, content, is_private: true } });
  }

  const q = 'INSERT INTO articles (title, author, content, is_private, password) VALUES (?, ?, ?, ?, ?)';
  const pwToStore = privateFlag ? password : null;

  try {
    const result = await dbQuery(q, [title, author, content, privateFlag, pwToStore]);
    const insertedId = result.insertId;
    res.redirect(`/article/${insertedId}`);
  } catch (err) {
    console.error('Error inserting article:', err);
    res.status(500).send('Server error');
  }
});

app.get('/article/:id', async (req, res) => {
  const id = req.params.id;
  try {
    const q = 'SELECT id, title, author, content, is_private FROM articles WHERE id = ? LIMIT 1';
    const rows = await dbQuery(q, [id]);
    if (!rows || rows.length === 0) {
      return res.status(404).send('Article not found');
    }
    const article = rows[0];
    if (article.is_private) {
      return res.render('article', { article: { id: article.id, title: article.title, author: article.author }, showPasswordForm: true, error: null });
    } else {
      const contentHtml = escapeAndFormat(article.content);
      return res.render('article', { article: { id: article.id, title: article.title, author: article.author, contentHtml }, showPasswordForm: false, error: null });
    }
  } catch (err) {
    console.error('Error getting article:', err);
    res.status(500).send('Server error');
  }
});

app.post('/article/:id', async (req, res) => {
  const id = req.params.id;
  const { password } = req.body;

  const q = 'SELECT id, title, author, content FROM articles WHERE id = ? AND is_private = 1 AND password = ? LIMIT 1';
  try {
    const rows = await dbQuery(q, [id, password]);
    if (!rows || rows.length === 0) {
      const metaQ = 'SELECT id, title, author FROM articles WHERE id = ? LIMIT 1';
      const metaRows = await dbQuery(metaQ, [id]);
      const meta = (metaRows && metaRows[0]) ? metaRows[0] : { id, title: 'Unknown', author: '' };
      return res.render('article', { article: meta, showPasswordForm: true, error: 'Wrong Password.' });
    }

    const article = rows[0];
    const contentHtml = escapeAndFormat(article.content);
    return res.render('article', { article: { id: article.id, title: article.title, author: article.author, contentHtml }, showPasswordForm: false, error: null });
  } catch (err) {
    console.error('Error verifying password:', err);
    res.status(500).send('Server error');
  }
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`Server running on http://localhost:${PORT}`);
});
