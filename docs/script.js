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

// Hide a card's video player if its file is missing or can't be played
document.querySelectorAll('.card video').forEach((video) => {
  const hide = () => video.remove();
  video.addEventListener('error', hide);
  if (video.error || video.networkState === HTMLMediaElement.NETWORK_NO_SOURCE) hide();
});
