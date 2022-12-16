FROM python:3.10
COPY ./requirements.txt /flaskproject/requirements.txt
WORKDIR /flaskproject
RUN pip install -r requirements.txt
COPY . /flaskproject
CMD ["python", "app.py"]
