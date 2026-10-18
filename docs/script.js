// Project filtering (All / C++ / Python)
const buttons = document.querySelectorAll('.filter');
const cards = document.querySelectorAll('.card');

buttons.forEach((btn) => {
  btn.addEventListener('click', () => {
    buttons.forEach((b) => b.classList.remove('active'));
    btn.classList.add('active');

    const filter = btn.dataset.filter;
    cards.forEach((card) => {
      const langs = card.dataset.lang.split(' ');
      const show = filter === 'all' || langs.includes(filter);
      card.classList.toggle('hidden', !show);
    });
  });
});

// Footer year
document.getElementById('year').textContent = new Date().getFullYear();

// If a card's video file is missing or can't be played, remove the player so the
// gradient behind it shows instead. (Only react to a real error event: checking the
// loading state at start-up is unreliable when this script is served from cache.)
document.querySelectorAll('.card video').forEach((video) => {
  video.addEventListener('error', () => video.remove());
  if (video.error) video.remove();
});
