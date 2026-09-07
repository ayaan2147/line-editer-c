# Simple Line Editor in C

## Team Members

- mohammed ayaan khalid shaikh
- om sharma

## Description

A simple command-line line editor written in C.

The editor stores lines of text in memory and allows the user
to insert, delete, display, save and load lines.

## Data Structure

The program uses a 2D character array:

char document[100][200];

Each row represents one line of the document.

## Features

- Insert a line
- Delete a line
- Display the document
- Save document to a text file
- Load document from a text file
- Invalid input handling
- Empty document handling

## How to Compile

```bash
gcc main.c -o editor