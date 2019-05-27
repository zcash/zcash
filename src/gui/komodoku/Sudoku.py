#!/usr/bin/env python

# Copyright (C) 2010 Paul Bourke <pauldbourke@gmail.com>
# Copyright (C) 2019 Anton Lysakov <tlysakov@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import pygame
import sys
import random
import sudoku_kmdlib
import time

class PyGameBoard():
  """Represents the game's frontend using pygame"""

  def __init__(self, engine, windowSize, gridValues, timestampValues):
    pygame.init()
    pygame.display.set_caption('Sudoku')
    self.__engine = engine
    self.__gridValues = gridValues
    self.__timestampValues = timestampValues
    self.__screen = pygame.display.set_mode(windowSize)
    background = pygame.image.load(sys.path[0] + '/background.png').convert()
    board = pygame.image.load(sys.path[0] + '/board.png')
    boardX = boardY = 10
    self.__screen.blit(background, (0, 0))
    self.__screen.blit(board, (boardX, boardY))
    self.__tiles = self.__createTiles(boardX, boardY)
    self.__drawUI()
    self.__draw()

  def __draw(self):
    """Handles events and updates display buffer"""
    while True:
      for event in pygame.event.get():
        if event.type == pygame.QUIT:
          sys.exit()
        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
          self.__handleMouse(event.pos)
        elif (event.type == pygame.KEYUP):
          self.__handleKeyboard(event.key)
      pygame.display.flip()

  def __drawUI(self):
    '''Draws the text buttons along the right panel'''
    font = pygame.font.Font(sys.path[0] + '/Roboto-Light.ttf', 28)
    font.set_underline(True)
    self.__titleText = font.render('Sudoku', 1, (0, 0, 0))
    self.__titleTextRect = self.__titleText.get_rect()
    self.__titleTextRect.centerx = 445
    self.__titleTextRect.centery = 30
    self.__screen.blit(self.__titleText, self.__titleTextRect)

    font = pygame.font.Font(sys.path[0] + '/Roboto-Light.ttf', 14)
    self.__titleText = font.render('TonyL 2019', 1, (0, 0, 0))
    self.__titleTextRect = self.__titleText.get_rect()
    self.__titleTextRect.centerx = 445
    self.__titleTextRect.centery = 55
    self.__screen.blit(self.__titleText, self.__titleTextRect)

    font = pygame.font.Font(sys.path[0] + '/Roboto-Light.ttf', 24)
    self.__newGameText = font.render('-New Game-', 1, (0, 0, 0))
    self.__newGameTextRect = self.__newGameText.get_rect()
    self.__newGameTextRect.centerx = 495
    self.__newGameTextRect.centery = 180
    self.__screen.blit(self.__newGameText, self.__newGameTextRect)

    self.__solveText = font.render('-Check Balance-', 1, (0, 0, 0))
    self.__solveTextRect = self.__solveText.get_rect()
    self.__solveTextRect.centerx = 495
    self.__solveTextRect.centery = 220
    self.__screen.blit(self.__solveText, self.__solveTextRect)

    font = pygame.font.Font(sys.path[0] + '/Roboto-Light.ttf', 24)
    self.__checkText = font.render('-Check Solution-', 1, (0, 0, 0))
    self.__checkTextRect = self.__checkText.get_rect()
    self.__checkTextRect.centerx = 495
    self.__checkTextRect.centery = 260
    self.__screen.blit(self.__checkText, self.__checkTextRect)

  def __handleKeyboard(self, key):
    """Get key pressed and update the game board"""
    validKeys = {pygame.K_0: "0", pygame.K_1: "1", pygame.K_2: "2",
                 pygame.K_3: "3", pygame.K_4: "4", pygame.K_5: "5",
                 pygame.K_6: "6", pygame.K_7: "7", pygame.K_8: "8",
                 pygame.K_9: "9", pygame.K_BACKSPACE: "", pygame.K_DELETE: ""}
    if key == pygame.K_ESCAPE:
      sys.exit()
    elif key in validKeys:
      i = self.__currentTile.getGridLoc()[0]
      j = self.__currentTile.getGridLoc()[1]
      cell_num = 9 * i + (j + 1)
      self.__currentTile.setFontColor(pygame.color.THECOLORS['blue'])
      self.__currentTile.updateValue(validKeys[key])
      self.__gridValues[i][j] = self.__currentTile.getValue()
      self.__timestampValues[cell_num] = int(round(time.time()))

  def __handleMouse(self, (x, y)):
    for row in self.__tiles:
      for tile in row:
        if tile.getRect().collidepoint(x, y):
          if not tile.isReadOnly():
            tile.highlight(pygame.color.THECOLORS['lightyellow'])
            if self.__currentTile.isCorrect():
              self.__currentTile.unhighlight()
            else:
              self.__currentTile.highlight((255, 164, 164))
            self.__currentTile = tile
    if self.__newGameTextRect.collidepoint(x, y):
      self.__engine.startNewGame()
    elif self.__solveTextRect.collidepoint(x, y):
        self.__engine.getSolution()
    elif self.__checkTextRect.collidepoint(x, y):
      ret = self.__engine.checkSolution(self.__gridValues, self.__timestampValues)

  def __updateBoard(self, gridValues):
    for i in range(9):
      for j in range(9):
        self.__tiles[i][j].updateValue(gridValues[i][j])

  def __unhightlightBoard(self):
    for i in range(9):
      for j in range(9):
        self.__tiles[i][j].unhighlight()

  def __createTiles(self, initX=0, initY=0):
    """Set up a list of tiles corresponding to the grid, along with
       each ones location coordinates on the board"""
    square_size = 40
    tiles = list()
    x = y = 0
    for i in range(0, 9):
      row = list()
      for j in range(0, 9):
        if j in (0, 1, 2):
          x = (j * 41) + (initX + 2)
        if j in (3, 4, 5):
          x = (j * 41) + (initX + 6)
        if j in (6, 7, 8):
          x = (j * 41) + (initX + 10)
        if i in (0, 1, 2):
          y = (i * 41) + (initY + 2)
        if i in (3, 4, 5):
          y = (i * 41) + (initY + 6)
        if i in (6, 7, 8):
          y = (i * 41) + (initY + 10)
        tile = Tile(self.__gridValues[i][j], (x, y), (i, j), square_size)
        row.append(tile)
      tiles.append(row)
    self.__currentTile = tiles[0][0]
    return tiles


class Tile():
  """Represents a graphical tile on the board"""

  def __init__(self, value, coords, gridLoc, size):
    xpos = coords[0]
    ypos = coords[1]
    self.__fontColor = pygame.color.THECOLORS["black"]
    self.__readOnly = False
    self.__colorSquare = pygame.Surface((size, size)).convert()
    self.__colorSquare.fill(pygame.color.THECOLORS['white'], None, pygame.BLEND_RGB_ADD)
    self.__colorSquareRect = self.__colorSquare.get_rect()
    self.__colorSquareRect = self.__colorSquareRect.move(xpos + 1, ypos + 1)
    self.__value = value
    self.__gridLoc = gridLoc
    self.__screen = pygame.display.get_surface()
    self.__rect = pygame.Rect(xpos, ypos, size, size)
    self.__isCorrect = True
    if self.__value is not '-':
      self.__readOnly = True
    self.__draw()

  def updateValue(self, value):
    self.__value = value
    self.__draw()

  def isCorrect(self):
    return self.__isCorrect

  def setCorrect(self, isCorrect):
    self.__isCorrect = isCorrect

  def setFontColor(self, fontColor):
    self.__fontColor = fontColor

  def getValue(self):
    return self.__value

  def getRect(self):
    return self.__rect

  def getGridLoc(self):
    return self.__gridLoc

  def isReadOnly(self):
    return self.__readOnly

  def highlight(self, color):
    if self.__readOnly is True:
      return
    self.__colorSquare.fill(color)
    self.__draw()

  def unhighlight(self):
    self.__colorSquare.fill((255, 225, 255), None, pygame.BLEND_RGB_ADD)
    self.__draw()

  def __draw(self):
    value = self.__value
    if self.__value == '-':
      value = ''
    font = pygame.font.Font(sys.path[0] + '/Roboto-Light.ttf', 24)
    text = font.render(str(value), 1, self.__fontColor)
    textpos = text.get_rect()
    textpos.centerx = self.__rect.centerx
    textpos.centery = self.__rect.centery
    self.__screen.blit(self.__colorSquare, self.__colorSquareRect)
    self.__screen.blit(text, textpos)


class Sudoku:
  """Represents the game's backend and logic"""

  def __init__(self, puzzleFile, rpc_connection):
    self.__puzzleFile = puzzleFile
    self.__rpc_connection = rpc_connection
    self.startNewGame()

  def startNewGame(self):
    self.__linePuzzle = self.__loadPuzzle(self.__puzzleFile)
    gridValues = self.lineToGrid(self.__linePuzzle)
    # prefill 0 timestamps for already known numbers
    timestampValues = self.prefill_timestamps(gridValues)
    board = PyGameBoard(self, (600, 400), gridValues, timestampValues)
    board.setValues(gridValues)

  def __loadPuzzle(self, listName):
    self.__chosen_puzzle = random.choice(listName)
    puzzle = self.__rpc_connection.cclib("txidinfo", "17", '"%22' + self.__chosen_puzzle  + '%22"')["unsolved"]
    print "Puzzle ID: " + self.__chosen_puzzle
    print "Reward amount: " + str(self.__rpc_connection.cclib("txidinfo", "17", '"%22' + self.__chosen_puzzle  + '%22"')["amount"])
    ret = []
    linePuzzle = str(puzzle)
    for i in linePuzzle:
      ret.append(i)
    return ret

  def gridToLine(self, grid):
    linePuzzle = ''
    for i in range(9):
      for j in range(9):
        linePuzzle += grid[i][j]
    return linePuzzle

  def lineToGrid(self, linePuzzle):
    assert (len(linePuzzle) == 81)
    grid = []
    for i in xrange(0, 81, 9):
      grid.append(linePuzzle[i:i + 9])
    return grid

  def getSolution(self):
    balance = self.__rpc_connection.cclibaddress("17")["mybalance"]
    print "Your balance: " + str(balance)

  def __solve(self, linePuzzle):
    linePuzzle = ''.join(linePuzzle)
    i = linePuzzle.find('-')
    if i == -1:
      return linePuzzle

    excluded_numbers = set()
    for j in range(81):
      if self.sameRow(i, j) or self.sameCol(i, j) or self.sameBlock(i, j):
        excluded_numbers.add(linePuzzle[j])

    for m in '123456789':
      if m not in excluded_numbers:
        funcRet = self.__solve(linePuzzle[:i] + m + linePuzzle[i + 1:])
        if funcRet is not None:
          return funcRet

  def prefill_timestamps(self, grid):
    timestamps = {}
    for i in range(9):
      for j in range(9):
          if grid[i][j] != '-':
              cell_num = 9 * i + ( j + 1 )
              timestamps[cell_num] = 0
    return timestamps

  def sameRow(self, i, j):
    return (i / 9 == j / 9)

  def sameCol(self, i, j):
    return (i - j) % 9 == 0

  def sameBlock(self, i, j):
    return (i / 27 == j / 27 and i % 9 / 3 == j % 9 / 3)

  def checkSolution(self, attemptGrid, timestampValues):
    # [%22<txid>%22,%22<solution>%22,t0,t1,t2,...]
    attemptLine = self.gridToLine(attemptGrid)

    #print attemptLine
    #print timestampValues
    timestampsline = ""
    for timestamp in timestampValues.values():
        timestampsline += ","
        timestampsline += str(timestamp)
    arg_line = "[%22"+self.__chosen_puzzle+"%22,%22"+attemptLine+"%22"+timestampsline+"]"
    print arg_line
    try:
        solution_info = self.__rpc_connection.cclib("solution", "17", '"' + arg_line + '"')
        print solution_info
        solution_txid = self.__rpc_connection.sendrawtransaction(solution_info["hex"])
        print "Solution accepted!"
        print solution_txid
    except Exception as e:
        print(e)
        print(solution_info)
        solution_txid = 'error'
    return solution_txid

def main():
  while True:
    # Assetchain hardcoded here
    chain = 'SUDOKU'
    try:
      print 'Welcome to the Komodo SudokuCC'
      rpc_connection = sudoku_kmdlib.def_credentials(chain)
      pending_puzzles = rpc_connection.cclib("pending", "17")["pending"]
      puzzle_list = []
      for puzzle in pending_puzzles:
        puzzle_list.append(puzzle["txid"])

    except Exception as e:
      #print rpc_connection
      print e
      print 'Cant connect to SUDOKU Daemon! Please re-check if it up'
      sys.exit()
    else:
      print 'Succesfully connected!\n'
      break
  newGame = Sudoku(puzzle_list, rpc_connection)

if __name__ == '__main__':
  main()
