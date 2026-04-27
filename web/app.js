const API = "/api/bank";
const TOKEN_KEY = "cyberbank.token";

const $ = (sel) => document.querySelector(sel);
const setStatus = (msg, kind = "") => {
  const el = $("#status");
  el.textContent = msg || "";
  el.className = kind;
};

async function api(action, body = {}) {
  const token = localStorage.getItem(TOKEN_KEY);
  const res = await fetch(API, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
    },
    body: JSON.stringify({ action, ...body }),
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || `Request failed (${res.status})`);
  return data;
}

function showLoggedIn(account) {
  $("#auth").hidden = true;
  $("#dashboard").hidden = false;
  $("#session").textContent = `${account.ownerName} · #${account.accountNumber}`;
  $("#balance").textContent = account.balance.toFixed(2);
  renderHistory(account.history || []);
}

function showLoggedOut() {
  localStorage.removeItem(TOKEN_KEY);
  $("#auth").hidden = false;
  $("#dashboard").hidden = true;
  $("#session").textContent = "";
}

function el(tag, opts = {}) {
  const node = document.createElement(tag);
  if (opts.className) node.className = opts.className;
  if (opts.text != null) node.textContent = opts.text;
  return node;
}

function renderHistory(items) {
  const ul = $("#history");
  ul.replaceChildren();
  if (!items.length) {
    const li = el("li");
    li.appendChild(el("span", { className: "meta", text: "No transactions yet." }));
    ul.appendChild(li);
    return;
  }
  for (const t of items.slice().reverse()) {
    const li = el("li");
    const left = el("div");
    left.appendChild(el("strong", { text: t.type }));
    if (t.note) {
      left.appendChild(document.createTextNode(" "));
      left.appendChild(el("span", { className: "meta", text: `(${t.note})` }));
    }
    left.appendChild(el("br"));
    left.appendChild(el("span", { className: "meta", text: t.timestamp }));

    const right = el("div", { text: `$${Number(t.amount).toFixed(2)}` });

    li.appendChild(left);
    li.appendChild(right);
    ul.appendChild(li);
  }
}

async function refresh() {
  try {
    const account = await api("me");
    showLoggedIn(account);
  } catch {
    showLoggedOut();
  }
}

$("#loginForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  setStatus("Signing in…");
  const fd = new FormData(e.target);
  try {
    const { token, account } = await api("login", {
      accountNumber: Number(fd.get("accountNumber")),
      pin: fd.get("pin"),
    });
    localStorage.setItem(TOKEN_KEY, token);
    showLoggedIn(account);
    setStatus("Welcome back.", "ok");
    e.target.reset();
  } catch (err) {
    setStatus(err.message, "error");
  }
});

$("#createForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  setStatus("Creating account…");
  const fd = new FormData(e.target);
  try {
    const { account } = await api("createAccount", {
      name: fd.get("name"),
      pin: fd.get("pin"),
    });
    setStatus(`Account #${account.accountNumber} created. Log in to continue.`, "ok");
    e.target.reset();
  } catch (err) {
    setStatus(err.message, "error");
  }
});

$("#logoutBtn").addEventListener("click", () => {
  showLoggedOut();
  setStatus("Logged out.");
});

for (const [id, action] of [["depositForm", "deposit"], ["withdrawForm", "withdraw"]]) {
  $(`#${id}`).addEventListener("submit", async (e) => {
    e.preventDefault();
    const fd = new FormData(e.target);
    try {
      const account = await api(action, { amount: Number(fd.get("amount")) });
      showLoggedIn(account);
      setStatus(`${action} succeeded.`, "ok");
      e.target.reset();
    } catch (err) {
      setStatus(err.message, "error");
    }
  });
}

$("#transferForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  const fd = new FormData(e.target);
  try {
    const account = await api("transfer", {
      to: Number(fd.get("to")),
      amount: Number(fd.get("amount")),
    });
    showLoggedIn(account);
    setStatus(`Sent $${Number(fd.get("amount")).toFixed(2)} to #${fd.get("to")}.`, "ok");
    e.target.reset();
  } catch (err) {
    setStatus(err.message, "error");
  }
});

refresh();
