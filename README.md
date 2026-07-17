# Caracterizaci-n-Proyecto-Tanque-y-Control-Multivariable_ESP32
Proyecto de caracterización y control multivariable mediante ESP32 y sensores ultrasónicos HC-SR04.
# Identificación Experimental de Procesos Aplicada al Proyecto Integrador

Este repositorio documenta el proceso de **Caracterización Experimental de Procesos** para un sistema hidráulico de dos tanques, realizado como parte de la asignatura **Sistemas de Control II** en el Departamento de Ingeniería Electrónica de la **Universidad Nacional Experimental del Táchira (UNET)**.

---

## 📋 Descripción del Proyecto

El objetivo principal de este trabajo fue realizar la caracterización física y dinámica de un prototipo de control de nivel, estableciendo una relación precisa entre el mundo físico (prototipo experimental) y el mundo virtual (simulación en Aspen HYSYS).

### Integrantes
* **José David Vivas Bautista** (V-30.202.000)
* **José David Fortoul Mogollón** (V-30.443.006)

---

## 🛠 Componentes del Sistema

1.  **Sensor de Nivel**: Módulo ultrasónico **HC-SR04** (alimentación 5V vía ESP32).
2.  **Elemento Final de Control**: Válvula manual de descarga rápida.
3.  **Sistema de Procesamiento**: Microcontrolador **ESP32** para monitoreo y servidor web local.
4.  **Plataforma de Simulación**: **Aspen HYSYS** (configurado para modelado dinámico).

---

## 🔬 Resultados Destacados

### 1. Caracterización del Sensor Ultrasónico
Se determinó la precisión y el rango útil del sensor HC-SR04:
* **Rango operativo seguro**: 0.0 cm a 13.0 cm.
* **Zona muerta**: Se detectó una limitación física de lectura por debajo de los 2.3 cm.
* **Precisión**: Error estático absoluto ≤ 0.3 cm.

### 2. Caracterización de la Descarga Hidráulica
Se analizaron cuatro puntos de apertura de válvula (25%, 50%, 75%, 100%) bajo la Ley de Torricelli, determinando los coeficientes de descarga (Cd) reales para cada caso y demostrando el comportamiento no lineal del sistema.

### 3. Dinámica de Tanques Interconectados
Se evaluó el acoplamiento hidráulico de un sistema de alimentación común. Se concluyó que existen asimetrías significativas (diferencias del 38.6% en el llenado) incluso con un distribuidor simétrico, debido a pérdidas de carga locales.

---

## 💻 Integración con Aspen HYSYS

El modelo de simulación fue calibrado utilizando datos experimentales:
* **Configuración del Tanque (V-100)**: Geometría de cilindro vertical (D=10.5cm, H=17.5cm).
* **Configuración de Válvula (VLV-100)**: Uso de tablas de usuario ("User Table") para mapear el comportamiento no lineal real (Cd) obtenido experimentalmente.
* **Termodinámica**: Condiciones fijadas a 25°C y 1 atm para asegurar la consistencia física.

---

## 📚 Conclusiones Técnicas
* La ubicación concéntrica del sensor ultrasónico es crítica para evitar interferencias por ecos falsos.
* La asimetría en el reparto de flujo en sistemas de alimentación común debe ser cuantificada experimentalmente para diseños de control robustos.
* El mapeo experimental de las características de la válvula es necesario para que el simulador (HYSYS) replique con fidelidad el comportamiento dinámico del prototipo físico.

---
*Universidad Nacional Experimental del Táchira (UNET) - 2026*
