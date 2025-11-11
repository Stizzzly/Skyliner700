// renderer.h — абстрактный интерфейс для рендерера

#ifndef RENDERER_H
#define RENDERER_H

#include <windows.h>

// Инициализация рендерера (привязка к окну)
int Renderer_Init(HWND hWnd);

// Очистка ресурсов
void Renderer_Shutdown();

// Начало кадра — очистка экрана
void Renderer_BeginFrame();

// Конец кадра — вывод на экран
void Renderer_EndFrame();

// Установка матрицы мира (позиция + вращение объекта)
void Renderer_SetWorldMatrix(float x, float y, float z, float rotY);

// Установка камеры (один раз при старте или при смене режима)
void Renderer_SetupCamera();

// Создание меша из массива вершин
int Renderer_CreateMesh(void* vertices, int vertexCount, int vertexStride, DWORD fvf);

// Отрисовка текущего меша
void Renderer_RenderMesh();

#endif
