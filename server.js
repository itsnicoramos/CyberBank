import http from "node:http";
import fs from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  randomBytes,
  scrypt,
  timingSafeEqual,
  createHmac,
} from "node:crypto";
import { promisify } from "node:util";

const scryptAsync = promisify(scrypt);

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.env.PORT) || 3000;
const STATE_PATH = path.join(__dirname, "bank-state.json");
const SECRET_PATH = path.join(__dirname, ".bank-secret");
const WEB_DIR = path.join(__dirname, "web");
const SESSION_TTL_SECONDS = 60 * 60 * 8;
const MAX_BODY_BYTES = 16 * 1024;
const MAX_FAILED_ATTEMPTS = 5;
const LOCKOUT_SECONDS = 60;

async function loadSecret() {
  if (process.env.BANK_SECRET) return process.env.BANK_SECRET;
  try {
    const existing = (await fs.readFile(SECRET_PATH, "utf8")).trim();
    if (existing) return existing;
  } catch (err) {
    if (err.code !== "ENOENT") throw err;
  }
  const secret = randomBytes(32).toString("hex");
  await fs.writeFile(SECRET_PATH, secret + "\n", { mode: 0o600 });
  console.log(`Generated session secret at ${SECRET_PATH}`);
  return secret;
}
const SECRET = await loadSecret();

async function loadState() {
  try {
    const raw = await fs.readFile(STATE_PATH, "utf8");
    return JSON.parse(raw);
  } catch (err) {
    if (err.code === "ENOENT") return { nextAccountNumber: 1001, accounts: {} };
    throw err;
  }
}
async function saveState(state) {
  await fs.writeFile(STATE_PATH, JSON.stringify(state, null, 2));
}

let lockChain = Promise.resolve();
function withLock(fn) {
  const next = lockChain.then(fn, fn);
  lockChain = next.then(() => {}, () => {});
  return next;
}

async function hashPin(pin, saltHex) {
  const salt = Buffer.from(saltHex, "hex");
  const buf = await scryptAsync(pin, salt, 32, { N: 16384, r: 8, p: 1 });
  return buf.toString("hex");
}
async function verifyPinHash(pin, account) {
  const expected = Buffer.from(account.pinHash, "hex");
  const actualHex = await hashPin(pin, account.pinSalt);
  const actual = Buffer.from(actualHex, "hex");
  return expected.length === actual.length && timingSafeEqual(expected, actual);
}

function b64url(buf) { return Buffer.from(buf).toString("base64url"); }
function signToken(payload) {
  const body = b64url(JSON.stringify({
    ...payload,
    exp: Math.floor(Date.now() / 1000) + SESSION_TTL_SECONDS,
  }));
  const sig = b64url(createHmac("sha256", SECRET).update(body).digest());
  return `${body}.${sig}`;
}
function verifyToken(token) {
  if (!token) return null;
  const [body, sig] = token.split(".");
  if (!body || !sig) return null;
  const expected = b64url(createHmac("sha256", SECRET).update(body).digest());
  if (sig.length !== expected.length) return null;
  if (!timingSafeEqual(Buffer.from(sig), Buffer.from(expected))) return null;
  try {
    const payload = JSON.parse(Buffer.from(body, "base64url").toString());
    if (typeof payload.sub !== "number") return null;
    if (payload.exp && payload.exp < Math.floor(Date.now() / 1000)) return null;
    return payload;
  } catch { return null; }
}

function publicAccount(a) {
  return {
    accountNumber: a.accountNumber,
    ownerName: a.ownerName,
    balance: round2(a.balance),
    history: a.history,
  };
}
const round2 = (n) => Math.round(n * 100) / 100;
const nowStamp = () => new Date().toISOString().replace("T", " ").slice(0, 19);

function readJson(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let total = 0;
    let aborted = false;
    req.on("data", (c) => {
      if (aborted) return;
      total += c.length;
      if (total > MAX_BODY_BYTES) {
        aborted = true;
        req.destroy();
        const e = new Error("Request body too large");
        e.statusCode = 413;
        reject(e);
        return;
      }
      chunks.push(c);
    });
    req.on("end", () => {
      if (aborted) return;
      const text = Buffer.concat(chunks).toString("utf8");
      if (!text) return resolve({});
      try { resolve(JSON.parse(text)); }
      catch { reject(new Error("Invalid JSON")); }
    });
    req.on("error", (err) => { if (!aborted) reject(err); });
  });
}
function send(res, status, data) {
  if (res.headersSent) return;
  res.writeHead(status, { "Content-Type": "application/json" });
  res.end(JSON.stringify(data));
}

async function handleApi(req, res) {
  if (req.method !== "POST") return send(res, 405, { error: "Use POST" });

  let body;
  try { body = await readJson(req); }
  catch (err) {
    return send(res, err.statusCode || 400, { error: err.message });
  }

  const action = body.action;

  if (action === "createAccount") {
    const name = String(body.name || "").trim();
    const pin = String(body.pin || "");
    if (!name) return send(res, 400, { error: "Name required" });
    if (!/^\d{4}$/.test(pin)) return send(res, 400, { error: "PIN must be exactly 4 digits" });

    const saltHex = randomBytes(16).toString("hex");
    const pinHash = await hashPin(pin, saltHex);

    return await withLock(async () => {
      const state = await loadState();
      const num = state.nextAccountNumber++;
      state.accounts[num] = {
        accountNumber: num,
        ownerName: name,
        pinSalt: saltHex,
        pinHash,
        balance: 0,
        payees: {},
        history: [],
        failedAttempts: 0,
        lockedUntil: 0,
      };
      await saveState(state);
      return send(res, 200, { account: { accountNumber: num, ownerName: name } });
    });
  }

  if (action === "login") {
    const num = Number(body.accountNumber);
    const pin = String(body.pin || "");
    return await withLock(async () => {
      const state = await loadState();
      const acct = state.accounts[num];
      const now = Math.floor(Date.now() / 1000);
      if (!acct) return send(res, 401, { error: "Invalid credentials" });
      if (acct.lockedUntil && acct.lockedUntil > now) {
        const retry = acct.lockedUntil - now;
        res.setHeader("Retry-After", String(retry));
        return send(res, 429, {
          error: `Too many failed attempts. Try again in ${retry}s.`,
        });
      }
      const ok = await verifyPinHash(pin, acct);
      if (!ok) {
        acct.failedAttempts = (acct.failedAttempts || 0) + 1;
        if (acct.failedAttempts >= MAX_FAILED_ATTEMPTS) {
          acct.lockedUntil = now + LOCKOUT_SECONDS;
          acct.failedAttempts = 0;
        }
        await saveState(state);
        return send(res, 401, { error: "Invalid credentials" });
      }
      acct.failedAttempts = 0;
      acct.lockedUntil = 0;
      await saveState(state);
      return send(res, 200, {
        token: signToken({ sub: num }),
        account: publicAccount(acct),
      });
    });
  }

  const auth = req.headers["authorization"] || "";
  const token = auth.startsWith("Bearer ") ? auth.slice(7) : null;
  const session = verifyToken(token);
  if (!session) return send(res, 401, { error: "Unauthorized" });

  return await withLock(async () => {
    const state = await loadState();
    const acct = state.accounts[session.sub];
    if (!acct) return send(res, 404, { error: "Account missing" });

    if (action === "me") return send(res, 200, publicAccount(acct));

    if (action === "deposit" || action === "withdraw") {
      const amount = Number(body.amount);
      if (!(amount > 0)) return send(res, 400, { error: "Amount must be positive" });
      if (action === "withdraw" && amount > acct.balance)
        return send(res, 400, { error: "Insufficient funds" });
      acct.balance = round2(acct.balance + (action === "deposit" ? amount : -amount));
      acct.history.push({ type: action.toUpperCase(), amount, timestamp: nowStamp(), note: "" });
      await saveState(state);
      return send(res, 200, publicAccount(acct));
    }

    if (action === "transfer") {
      const to = Number(body.to);
      const amount = Number(body.amount);
      if (!(amount > 0)) return send(res, 400, { error: "Amount must be positive" });
      if (to === acct.accountNumber)
        return send(res, 400, { error: "Cannot transfer to your own account" });
      const dest = state.accounts[to];
      if (!dest) return send(res, 404, { error: "Recipient not found" });
      if (amount > acct.balance) return send(res, 400, { error: "Insufficient funds" });

      acct.balance = round2(acct.balance - amount);
      dest.balance = round2(dest.balance + amount);
      const ts = nowStamp();
      acct.history.push({ type: "TRANSFER_OUT", amount, timestamp: ts, note: `to=${to}` });
      dest.history.push({ type: "TRANSFER_IN", amount, timestamp: ts, note: `from=${acct.accountNumber}` });
      await saveState(state);
      return send(res, 200, publicAccount(acct));
    }

    return send(res, 400, { error: `Unknown action: ${action}` });
  });
}

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".ico": "image/x-icon",
};
const CSP =
  "default-src 'self'; script-src 'self'; style-src 'self'; " +
  "base-uri 'self'; form-action 'self'; frame-ancestors 'none'";

async function serveStatic(req, res) {
  const url = new URL(req.url, `http://${req.headers.host}`);
  let rel = decodeURIComponent(url.pathname);
  if (rel === "/" || rel === "") rel = "/index.html";
  const filePath = path.normalize(path.join(WEB_DIR, rel));
  if (filePath !== WEB_DIR && !filePath.startsWith(WEB_DIR + path.sep)) {
    res.writeHead(403); return res.end("Forbidden");
  }
  try {
    const data = await fs.readFile(filePath);
    const type = MIME[path.extname(filePath)] || "application/octet-stream";
    const headers = { "Content-Type": type };
    if (type.startsWith("text/html")) headers["Content-Security-Policy"] = CSP;
    res.writeHead(200, headers);
    res.end(data);
  } catch {
    res.writeHead(404); res.end("Not found");
  }
}

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (url.pathname === "/api/bank") return await handleApi(req, res);
    return await serveStatic(req, res);
  } catch (err) {
    console.error(err);
    send(res, 500, { error: "Internal server error" });
  }
});

server.listen(PORT, () => {
  console.log(`CyberBank server running at http://localhost:${PORT}`);
});
