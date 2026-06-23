FROM python:3.12-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY hologram_poller.py .

# Runs forever, polling on POLL_INTERVAL_S. Config comes from env (see
# docker-compose.yml + .env). HOLOGRAM_API_KEY must be supplied at runtime.
CMD ["python", "-u", "hologram_poller.py"]
